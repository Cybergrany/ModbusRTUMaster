#include <chrono>
#include <cstdint>
#include <iostream>

#include "Arduino.h"
#include "FixtureLock.h"

#include "ModbusRTUMaster.h"

namespace {

class FixtureStream : public Stream {
  public:
    int available() override { return 0; }
    int read() override { return -1; }
    size_t write(uint8_t) override { return 1; }
};

}  // namespace

int main() {
  const uint32_t warmupIterations = 250000;
  const uint32_t iterations = 250000;
  const uint16_t values[4] = {0x1122, 0x3344, 0x5566, 0x7788};
  FixtureStream stream;
  FixtureLock lock;
  ModbusRTUMaster master(stream, 4, 5);

  modbus_master_fixture::reset();
  modbus_master_fixture::state().benchmark_mode = true;
  volatile unsigned resultSum = 0;

  // Exercise the same path before sampling so host CPU frequency ramp-up and
  // first-use effects do not masquerade as a library regression.
  for (uint32_t i = 0; i < warmupIterations; ++i) {
    resultSum += master.fc69WriteMultipleHoldingRegisters(
        7, static_cast<uint16_t>(0x1200U + (i & 0x00FFU)),
        values, 4, &lock);
  }
  if (resultSum != 0 ||
      modbus_master_fixture::state().event_count != warmupIterations * 3ULL) {
    return 2;
  }
  modbus_master_fixture::reset();
  modbus_master_fixture::state().benchmark_mode = true;
  resultSum = 0;

  const std::chrono::steady_clock::time_point start =
      std::chrono::steady_clock::now();
  for (uint32_t i = 0; i < iterations; ++i) {
    resultSum += master.fc69WriteMultipleHoldingRegisters(
        7, static_cast<uint16_t>(0x1200U + (i & 0x00FFU)),
        values, 4, &lock);
  }
  const std::chrono::steady_clock::time_point finish =
      std::chrono::steady_clock::now();

  if (resultSum != 0 ||
      modbus_master_fixture::state().event_count != iterations * 3ULL) {
    return 2;
  }

  std::cout << std::chrono::duration_cast<std::chrono::microseconds>(
                   finish - start).count()
            << '\n';
  return 0;
}
