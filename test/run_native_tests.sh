#!/usr/bin/env bash
set -euo pipefail

test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${test_dir}/.." && pwd)
build_dir=$(mktemp -d /tmp/modbus-master-native.XXXXXX)
trap 'rm -rf -- "${build_dir}"' EXIT

cd "${repo_dir}"

sha256sum -c test/frozen/SHA256SUMS

common_flags=(
  -std=c++11
  -Wall
  -Wextra
  -Werror
  -Wno-unused-variable
  -pedantic
  '-DMBUS_RTU_MASTER_LOCK_HEADER="FixtureLock.h"'
  -DMBUS_RTU_MASTER_LOCK_TYPE=FixtureLock
  -Itest
  -Itest/support
)

# A historical/upstream Comm has a compatible writeAdu() signature but does not
# guarantee that a no-response frame delays the next TX. Prove that such a
# mismatched dependency tuple is rejected instead of compiling with subtly
# different broadcast ordering.
if g++ "${common_flags[@]}" \
    -DMBUS_TEST_OMIT_NO_RESPONSE_GATE_CAPABILITY=1 \
    -Isrc \
    -c test/comm_capability_compile.cpp \
    -o "${build_dir}/incompatible-comm.o" \
    > "${build_dir}/incompatible-comm.log" 2>&1; then
  echo "Master compiled without the required Comm no-response gate" >&2
  exit 1
fi
if ! grep -Fq 'MBUS_RTU_COMM_HAS_NO_RESPONSE_GATE=1' \
    "${build_dir}/incompatible-comm.log"; then
  cat "${build_dir}/incompatible-comm.log" >&2
  echo "Master failed for an unexpected reason in the Comm capability gate" >&2
  exit 1
fi

g++ "${common_flags[@]}" -Isrc \
  test/master_trace.cpp \
  test/support/ModbusADU.cpp \
  src/ModbusRTUMaster.cpp \
  -o "${build_dir}/candidate-trace"

g++ "${common_flags[@]}" \
  -DMBUS_USE_FROZEN_OGM_ORACLE=1 \
  -Itest/frozen/ogm_portable_2055adb \
  test/master_trace.cpp \
  test/support/ModbusADU.cpp \
  test/frozen/ogm_portable_2055adb/ModbusRTUMaster.cpp \
  -o "${build_dir}/oracle-trace"

"${build_dir}/candidate-trace" > "${build_dir}/candidate.trace"
"${build_dir}/oracle-trace" > "${build_dir}/oracle.trace"

# Candidate and frozen source must agree, and both must still match the trace
# captured from OGM_Portable 2055adb. The second comparison prevents an
# accidental edit to both test paths from silently redefining compatibility.
diff -u "${build_dir}/oracle.trace" "${build_dir}/candidate.trace"
diff -u test/fixtures/ogm_portable_2055adb_master.trace \
  "${build_dir}/candidate.trace"

codegen_flags=(
  "${common_flags[@]}"
  -Os
  -fno-exceptions
  -fno-rtti
  -ffunction-sections
  -fdata-sections
  -fstack-usage
)

g++ "${codegen_flags[@]}" -Isrc \
  -c src/ModbusRTUMaster.cpp \
  -o "${build_dir}/candidate.o"
g++ "${codegen_flags[@]}" \
  -Itest/frozen/ogm_portable_2055adb \
  -c test/frozen/ogm_portable_2055adb/ModbusRTUMaster.cpp \
  -o "${build_dir}/oracle.o"

nm -S --size-sort --defined-only "${build_dir}/candidate.o" \
  | awk '{print $2, $3, $4}' > "${build_dir}/candidate.nm"
nm -S --size-sort --defined-only "${build_dir}/oracle.o" \
  | awk '{print $2, $3, $4}' > "${build_dir}/oracle.nm"
diff -u "${build_dir}/oracle.nm" "${build_dir}/candidate.nm"

# Size parity alone cannot detect a same-sized instruction substitution. Compare
# the complete disassembly and relocation stream, omitting only objdump's input
# filename banner so the differently named object files remain comparable.
objdump -drwC "${build_dir}/candidate.o" \
  | sed -n '/^Disassembly of section/,$p' \
  > "${build_dir}/candidate.disassembly"
objdump -drwC "${build_dir}/oracle.o" \
  | sed -n '/^Disassembly of section/,$p' \
  > "${build_dir}/oracle.disassembly"
diff -u "${build_dir}/oracle.disassembly" \
  "${build_dir}/candidate.disassembly"

# GCC prefixes each stack-usage record with the source path and line number.
# Those locations intentionally differ between src/ and the frozen oracle, so
# strip only that location and require every function's remaining stack record
# (bytes plus static/dynamic classification) to be identical.
sed -E 's|^[^:]+:[0-9]+:[0-9]+:||' "${build_dir}/candidate.su" \
  > "${build_dir}/candidate.normalized.su"
sed -E 's|^[^:]+:[0-9]+:[0-9]+:||' "${build_dir}/oracle.su" \
  > "${build_dir}/oracle.normalized.su"
diff -u "${build_dir}/oracle.normalized.su" \
  "${build_dir}/candidate.normalized.su"

candidate_size=$(size "${build_dir}/candidate.o" | awk 'NR == 2 {print $4}')
oracle_size=$(size "${build_dir}/oracle.o" | awk 'NR == 2 {print $4}')
if [[ "${candidate_size}" != "${oracle_size}" ]]; then
  echo "Object footprint changed: candidate=${candidate_size}, oracle=${oracle_size}" >&2
  exit 1
fi

candidate_stack_max=$(awk -F '\t' \
  'BEGIN { max = 0 } { if ($2 + 0 > max) max = $2 + 0 } END { print max }' \
  "${build_dir}/candidate.normalized.su")

if [[ "${MBUS_STRICT_PERFORMANCE:-0}" == "1" ]]; then
  benchmark_flags=("${common_flags[@]}" -O3 -DNDEBUG)

  g++ "${benchmark_flags[@]}" -Isrc \
    test/master_benchmark.cpp \
    test/support/ModbusADU.cpp \
    src/ModbusRTUMaster.cpp \
    -o "${build_dir}/candidate-benchmark"
  g++ "${benchmark_flags[@]}" \
    -DMBUS_USE_FROZEN_OGM_ORACLE=1 \
    -Itest/frozen/ogm_portable_2055adb \
    test/master_benchmark.cpp \
    test/support/ModbusADU.cpp \
    test/frozen/ogm_portable_2055adb/ModbusRTUMaster.cpp \
    -o "${build_dir}/oracle-benchmark"

  for iteration in 1 2 3 4 5 6 7; do
    "${build_dir}/oracle-benchmark" >> "${build_dir}/oracle-times"
    "${build_dir}/candidate-benchmark" >> "${build_dir}/candidate-times"
  done
  sort -n "${build_dir}/oracle-times" > "${build_dir}/oracle-times-sorted"
  sort -n "${build_dir}/candidate-times" > \
    "${build_dir}/candidate-times-sorted"
  oracle_median=$(sed -n '4p' "${build_dir}/oracle-times-sorted")
  candidate_median=$(sed -n '4p' "${build_dir}/candidate-times-sorted")

  # Timing is an intentionally opt-in host signal. The default disassembly and
  # stack gates are deterministic; this wider bound catches a material slowdown
  # while tolerating ordinary scheduler and CPU-frequency noise.
  if ! awk -v candidate="${candidate_median}" -v oracle="${oracle_median}" \
      'BEGIN { exit(candidate <= oracle * 1.20 ? 0 : 1) }'; then
    echo "Hot path slowed by more than 20%: candidate=${candidate_median}us, oracle=${oracle_median}us" >&2
    exit 1
  fi
  echo "Strict hot path: candidate=${candidate_median}us, oracle=${oracle_median}us"
fi

scenario_count=$(wc -l < test/fixtures/ogm_portable_2055adb_master.trace)
echo "Master compatibility: ${scenario_count} frozen trace scenarios matched"
echo "Master dependency: missing Comm no-response gate was rejected"
echo "Master codegen: exact disassembly, symbols, and ${candidate_size}-byte object footprint matched"
echo "Master stack: exact per-function usage matched (maximum ${candidate_stack_max} bytes)"
