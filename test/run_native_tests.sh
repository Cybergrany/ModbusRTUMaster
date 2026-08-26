#!/usr/bin/env bash
set -euo pipefail

test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${test_dir}/.." && pwd)
build_dir=$(mktemp -d /tmp/modbus-master-native.XXXXXX)
trap 'rm -rf -- "${build_dir}"' EXIT

cd "${repo_dir}"

sha256sum -c test/support/SHA256SUMS
python3 test/check_example_contracts.py
python3 test/check_package_contracts.py

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

# A transport can expose writeAdu() while still lacking the quiet-period gate
# required after no-response writes. Reject that dependency at compile time.
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
  -o "${build_dir}/master-trace"

"${build_dir}/master-trace" > "${build_dir}/master.trace"
diff -u test/fixtures/master_behavior.trace "${build_dir}/master.trace"

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
  -o "${build_dir}/master.o"

candidate_size=$(size "${build_dir}/master.o" | awk 'NR == 2 {print $4}')
max_object_size=${MBUS_RTU_MASTER_MAX_OBJECT_BYTES:-8435}
if (( candidate_size > max_object_size )); then
  echo "Object footprint exceeded budget: actual=${candidate_size}, max=${max_object_size}" >&2
  exit 1
fi

sed -E 's|^[^:]+:[0-9]+:[0-9]+:||' "${build_dir}/master.su" \
  > "${build_dir}/master.normalized.su"
candidate_stack_max=$(awk -F '\t' \
  'BEGIN { max = 0 } { if ($2 + 0 > max) max = $2 + 0 } END { print max }' \
  "${build_dir}/master.normalized.su")
max_stack_bytes=${MBUS_RTU_MASTER_MAX_STACK_BYTES:-400}
if (( candidate_stack_max > max_stack_bytes )); then
  echo "Stack usage exceeded budget: actual=${candidate_stack_max}, max=${max_stack_bytes}" >&2
  exit 1
fi

if [[ "${MBUS_STRICT_PERFORMANCE:-0}" == "1" ]]; then
  benchmark_flags=("${common_flags[@]}" -O3 -DNDEBUG)
  g++ "${benchmark_flags[@]}" -Isrc \
    test/master_benchmark.cpp \
    test/support/ModbusADU.cpp \
    src/ModbusRTUMaster.cpp \
    -o "${build_dir}/master-benchmark"

  benchmark_runner=()
  if command -v taskset >/dev/null 2>&1; then
    benchmark_cpu=${MBUS_RTU_MASTER_PERF_CPU:-}
    if [[ -z "${benchmark_cpu}" ]]; then
      allowed_cpus=$(taskset -pc $$ | sed 's/^.*: //')
      benchmark_cpu=${allowed_cpus%%,*}
      benchmark_cpu=${benchmark_cpu%%-*}
    fi
    if [[ ! "${benchmark_cpu}" =~ ^[0-9]+$ ]] ||
       ! taskset -c "${benchmark_cpu}" true >/dev/null 2>&1; then
      echo "MBUS_RTU_MASTER_PERF_CPU is not available to this process" >&2
      exit 2
    fi
    benchmark_runner=(taskset -c "${benchmark_cpu}")
    echo "Strict hot-path affinity: CPU ${benchmark_cpu}"
  elif [[ -n ${MBUS_RTU_MASTER_PERF_CPU:-} ]]; then
    echo "MBUS_RTU_MASTER_PERF_CPU requires taskset" >&2
    exit 2
  fi

  for iteration in 1 2 3 4 5 6 7; do
    "${benchmark_runner[@]}" "${build_dir}/master-benchmark" \
      >> "${build_dir}/benchmark-times"
  done
  sort -n "${build_dir}/benchmark-times" > "${build_dir}/benchmark-times-sorted"
  benchmark_median=$(sed -n '4p' "${build_dir}/benchmark-times-sorted")
  max_benchmark_us=${MBUS_RTU_MASTER_MAX_BENCHMARK_US:-40000}
  if (( benchmark_median > max_benchmark_us )); then
    echo "Hot path exceeded budget: median=${benchmark_median}us, max=${max_benchmark_us}us" >&2
    exit 1
  fi
  echo "Strict hot path: median=${benchmark_median}us, max=${max_benchmark_us}us"
fi

scenario_count=$(wc -l < test/fixtures/master_behavior.trace)
echo "Master behavior: ${scenario_count} golden trace scenarios matched"
echo "Master dependency: missing Comm no-response gate was rejected"
echo "Master footprint: ${candidate_size}/${max_object_size} object bytes"
echo "Master stack: ${candidate_stack_max}/${max_stack_bytes} bytes"
