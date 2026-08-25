#!/usr/bin/env bash
set -euo pipefail

test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${test_dir}/.." && pwd)
dependency_dir=$(mktemp -d /tmp/modbus-master-deps.XXXXXX)
comm_dir="${dependency_dir}/ModbusRTUComm-pinned"
adu_dir="${dependency_dir}/ModbusADU-pinned"
mkdir -p "${comm_dir}" "${adu_dir}"
trap 'rm -rf -- "${dependency_dir}"' EXIT

: "${PLATFORMIO_CORE_DIR:?Set PLATFORMIO_CORE_DIR to a writable PlatformIO core directory}"
: "${MBUS_RTU_COMM_SOURCE_DIR:?Set MBUS_RTU_COMM_SOURCE_DIR to the pinned Comm repository src directory}"

if [[ ! -f "${MBUS_RTU_COMM_SOURCE_DIR}/ModbusRTUComm.h" ]]; then
  echo "ModbusRTUComm.h not found in ${MBUS_RTU_COMM_SOURCE_DIR}" >&2
  exit 1
fi

expected_comm_sha=$(sed -nE \
  's|.*Cybergrany/ModbusRTUComm.git#([0-9a-f]{40}).*|\1|p' \
  "${repo_dir}/library.json")
if [[ -z "${expected_comm_sha}" ]]; then
  echo "Could not read the immutable Comm SHA from library.json" >&2
  exit 1
fi

comm_repo=$(git -C "${MBUS_RTU_COMM_SOURCE_DIR}" rev-parse --show-toplevel)
actual_comm_sha=$(git -C "${comm_repo}" rev-parse HEAD)
if [[ "${actual_comm_sha}" != "${expected_comm_sha}" ]]; then
  echo "Comm checkout mismatch: expected=${expected_comm_sha}, actual=${actual_comm_sha}" >&2
  exit 1
fi
if [[ -n "$(git -C "${comm_repo}" status --porcelain --untracked-files=all)" ]]; then
  echo "Comm checkout is dirty; the example gate requires exact pinned sources" >&2
  exit 1
fi

# The ADU files in support/ are byte-frozen from the immutable dependency SHA.
# Stage only those two files so the native Comm test double in the same folder
# can never leak into an embedded build. Stage Comm under a unique library name
# as well: both repositories call their source directory `src`, and passing the
# two directories directly would make PlatformIO collapse that name collision.
(cd "${repo_dir}" && sha256sum -c test/frozen/SHA256SUMS)
cp -R "${MBUS_RTU_COMM_SOURCE_DIR}/." "${comm_dir}/"
cp "${repo_dir}/test/support/ModbusADU.h" "${adu_dir}/ModbusADU.h"
cp "${repo_dir}/test/support/ModbusADU.cpp" "${adu_dir}/ModbusADU.cpp"

comm_production_support="${comm_repo}/test/production_support"
if [[ ! -f "${comm_production_support}/Arduino.h" ]]; then
  echo "Pinned Comm checkout is missing its production native Arduino shim" >&2
  exit 1
fi

# Link the actual Master, Comm and byte-frozen ADU production sources together
# under the oldest supported language mode. This catches header/capability and
# separate-library linkage defects before the slower architecture builds.
g++ \
  -std=c++11 \
  -Wall \
  -Wextra \
  -Werror \
  -pedantic \
  -DARDUINO=1 \
  -DMBUS_RTU_ALLOW_DIRECT_SERIAL_DIAGNOSTICS=0 \
  -DMBUS_RTU_PLATFORM_TRACE=0 \
  -DMBUS_RTU_ENABLE_RX_THREAD=0 \
  -I"${comm_production_support}" \
  -I"${comm_dir}" \
  -I"${adu_dir}" \
  -I"${repo_dir}/src" \
  "${repo_dir}/test/comm_capability_compile.cpp" \
  "${adu_dir}/ModbusADU.cpp" \
  "${comm_dir}/ModbusRTUComm.cpp" \
  "${repo_dir}/src/ModbusRTUMaster.cpp" \
  -o "${dependency_dir}/master-comm-native"
"${dependency_dir}/master-comm-native"
echo "Strict C++11 Master/Comm/ADU production tuple linked"

compile_example() {
  local board=$1
  local sketch=$2
  echo "Compiling ${sketch#${repo_dir}/} for ${board}"
  pio ci \
    --board "${board}" \
    --lib "${repo_dir}/src" \
    --lib "${comm_dir}" \
    --lib "${adu_dir}" \
    "${sketch}"
}

mbed_board=${MBUS_MBED_TEST_BOARD:-giga_r1_m7}
avr_board=${MBUS_AVR_TEST_BOARD:-nanoatmega328}

# The general example takes both its mbed HardwareSerial path and its AVR
# SoftwareSerial path. The interactive Probe is AVR-specific. The final two
# builds link every OGM extension through the dedicated tuple fixture.
compile_example "${mbed_board}" \
  "${repo_dir}/examples/ModbusRTUMasterExample/ModbusRTUMasterExample.ino"
compile_example "${avr_board}" \
  "${repo_dir}/examples/ModbusRTUMasterExample/ModbusRTUMasterExample.ino"
compile_example "${avr_board}" \
  "${repo_dir}/examples/ModbusRTUMasterProbe/ModbusRTUMasterProbe.ino"
compile_example "${mbed_board}" \
  "${repo_dir}/test/arduino_compile_gate/arduino_compile_gate.ino"
compile_example "${avr_board}" \
  "${repo_dir}/test/arduino_compile_gate/arduino_compile_gate.ino"

echo "Arduino examples and OGM API fixture compiled against Comm ${actual_comm_sha}"
