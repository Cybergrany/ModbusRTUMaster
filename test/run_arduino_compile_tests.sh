#!/usr/bin/env bash
set -euo pipefail

test_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "${test_dir}/.." && pwd)
adu_dir=$(mktemp -d /tmp/modbus-master-adu.XXXXXX)
trap 'rm -rf -- "${adu_dir}"' EXIT

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
# can never leak into an embedded build.
(cd "${repo_dir}" && sha256sum -c test/frozen/SHA256SUMS)
cp "${repo_dir}/test/support/ModbusADU.h" "${adu_dir}/ModbusADU.h"
cp "${repo_dir}/test/support/ModbusADU.cpp" "${adu_dir}/ModbusADU.cpp"

compile_example() {
  local board=$1
  local sketch=$2
  echo "Compiling ${sketch#${repo_dir}/} for ${board}"
  pio ci \
    --board "${board}" \
    --lib "${repo_dir}/src" \
    --lib "${MBUS_RTU_COMM_SOURCE_DIR}" \
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
