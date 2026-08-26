#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

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

std::string bytesToHex(const std::vector<uint8_t>& bytes) {
  std::ostringstream out;
  out << std::hex << std::setfill('0');
  for (size_t i = 0; i < bytes.size(); ++i) {
    if (i) out << ':';
    out << std::setw(2) << static_cast<unsigned>(bytes[i]);
  }
  return out.str();
}

std::string eventsToString() {
  using namespace modbus_master_fixture;
  std::ostringstream out;
  const std::vector<EventRecord>& events = state().events;
  for (size_t i = 0; i < events.size(); ++i) {
    if (i) out << ',';
    out << eventName(events[i].event);
    if (events[i].value) out << '(' << events[i].value << ')';
  }
  return out.str();
}

template <typename T>
std::string valuesToString(const T* values, size_t count) {
  std::ostringstream out;
  for (size_t i = 0; i < count; ++i) {
    if (i) out << ',';
    out << static_cast<unsigned long>(values[i]);
  }
  return out.str();
}

void emit(const char* name, ModbusRTUMasterError result,
          const std::string& observed = std::string(),
          uint8_t exception = 0) {
  std::cout << name
            << " result=" << static_cast<unsigned>(result)
            << " tx=" << bytesToHex(modbus_master_fixture::state().tx)
            << " events=" << eventsToString()
            << " observed=" << observed
            << " exception=" << static_cast<unsigned>(exception)
            << '\n';
}

void queue(uint8_t id, uint8_t functionCode,
           const std::initializer_list<uint8_t>& data) {
  const uint8_t* begin = data.size() ? data.begin() : nullptr;
  modbus_master_fixture::queueResponse(
      id, functionCode, begin, static_cast<uint16_t>(data.size()));
}

}  // namespace

int main() {
  using namespace modbus_master_fixture;

  FixtureStream stream;

  reset();
  ModbusRTUMaster lifecycle(stream, 4, 5);
  lifecycle.setTimeout(777);
  lifecycle.begin(250000, SERIAL_8N1);
  lifecycle.setPreTxGapUsOnce(1234);
  lifecycle.setPostTxGapUsOnce(5678);
  emit("lifecycle", MODBUS_RTU_MASTER_SUCCESS);

  ModbusRTUMaster master(stream, 4, 5);
  FixtureLock lock;

  reset();
  queue(7, 1, {2, 0x55, 0x01});
  bool coils[10] = {};
  ModbusRTUMasterError result = master.readCoils(7, 0x1234, coils, 10, &lock);
  emit("read_coils", result, valuesToString(coils, 10));

  reset();
  queue(8, 2, {1, 0xA6});
  bool inputs[8] = {};
  result = master.readDiscreteInputs(8, 0x0020, inputs, 8);
  emit("read_discrete", result, valuesToString(inputs, 8));

  reset();
  queue(9, 3, {4, 0x11, 0x22, 0x33, 0x44});
  uint16_t holding[2] = {};
  result = master.readHoldingRegisters(9, 0x0102, holding, 2, &lock);
  emit("read_holding", result, valuesToString(holding, 2));

  reset();
  queue(10, 4, {4, 0xAA, 0x55, 0x55, 0xAA});
  uint16_t inputRegisters[2] = {};
  result = master.readInputRegisters(10, 0x0203, inputRegisters, 2);
  emit("read_input", result, valuesToString(inputRegisters, 2));

  reset();
  queue(11, 5, {0x12, 0x34, 0xFF, 0x00});
  result = master.writeSingleCoil(11, 0x1234, true);
  emit("write_single_coil", result);

  reset();
  queue(12, 6, {0x22, 0x44, 0xBE, 0xEF});
  result = master.writeSingleHoldingRegister(12, 0x2244, 0xBEEF);
  emit("write_single_register", result);

  reset();
  result = master.writeSingleHoldingRegister(0, 0x0042, 0x1357);
  emit("broadcast_single_register", result);

  reset();
  bool multiCoils[10] = {true, false, true, true, false,
                         false, true, false, true, true};
  queue(13, 15, {0x10, 0x20, 0x00, 0x0A});
  result = master.writeMultipleCoils(13, 0x1020, multiCoils, 10, &lock);
  emit("write_multiple_coils", result);

  reset();
  uint16_t multiRegisters[3] = {0x1122, 0x3344, 0x5566};
  queue(14, 16, {0x20, 0x30, 0x00, 0x03});
  result = master.writeMultipleHoldingRegisters(
      14, 0x2030, multiRegisters, 3, &lock);
  emit("write_multiple_registers", result);

  reset();
  result = master.writeMultipleCoils(0, 0x3040, multiCoils, 10, &lock);
  emit("broadcast_multiple_coils", result);

  reset();
  result = master.fc69WriteSingleCoil(7, 0x1234, true);
  emit("fc69_single_coil", result);

  reset();
  result = master.fc69WriteSingleHoldingRegister(7, 0x1234, 0xBEEF);
  emit("fc69_single_register", result);

  reset();
  result = master.fc69WriteMultipleCoils(
      7, 0x1234, multiCoils, 10, &lock);
  emit("fc69_multiple_coils", result);

  reset();
  const uint16_t fc69Registers[2] = {0x1122, 0x3344};
  result = master.fc69WriteMultipleHoldingRegisters(
      7, 0x1234, fc69Registers, 2, &lock);
  emit("fc69_multiple_registers", result);

  reset();
  const uint8_t rawPdu[9] = {
      0x12, 0x34, 0x00, 0x02, 0x04, 0x11, 0x22, 0x33, 0x44};
  result = master.fc69WriteRaw(7, 16, rawPdu, sizeof(rawPdu), &lock);
  emit("fc69_raw", result);

  reset();
  state().write_result = false;
  uint16_t failedRead[1] = {};
  result = master.readHoldingRegisters(7, 0, failedRead, 1);
  emit("write_failure_read", result);

  reset();
  state().write_result = false;
  result = master.fc69WriteSingleCoil(7, 0, true);
  emit("write_failure_fc69", result);

  const ModbusRTUCommError commErrors[] = {
      MODBUS_RTU_COMM_TIMEOUT,
      MODBUS_RTU_COMM_FRAME_ERROR,
      MODBUS_RTU_COMM_CRC_ERROR,
      static_cast<ModbusRTUCommError>(99)};
  const char* commNames[] = {
      "comm_timeout", "comm_frame", "comm_crc", "comm_unknown"};
  for (size_t i = 0; i < 4; ++i) {
    reset();
    state().read_result = commErrors[i];
    result = master.readHoldingRegisters(7, 0, failedRead, 1);
    emit(commNames[i], result);
  }

  reset();
  queue(8, 3, {2, 0x12, 0x34});
  result = master.readHoldingRegisters(7, 0, failedRead, 1);
  emit("unexpected_id", result);

  reset();
  queue(7, 0x83, {2});
  result = master.readHoldingRegisters(7, 0, failedRead, 1);
  emit("exception", result, std::string(), master.getExceptionResponse());

  reset();
  queue(7, 4, {2, 0x12, 0x34});
  result = master.readHoldingRegisters(7, 0, failedRead, 1);
  emit("unexpected_function", result);

  reset();
  queue(7, 3, {2, 0x12, 0x34, 0x56});
  result = master.readHoldingRegisters(7, 0, failedRead, 1);
  emit("unexpected_read_length", result);

  reset();
  queue(7, 3, {1, 0x12, 0x34});
  result = master.readHoldingRegisters(7, 0, failedRead, 1);
  emit("unexpected_byte_count", result);

  reset();
  queue(7, 6, {0x00, 0x02, 0x12, 0x34});
  result = master.writeSingleHoldingRegister(7, 1, 0x1234);
  emit("unexpected_address", result);

  reset();
  queue(7, 6, {0x00, 0x01, 0x12, 0x35});
  result = master.writeSingleHoldingRegister(7, 1, 0x1234);
  emit("unexpected_value", result);

  reset();
  queue(7, 16, {0x00, 0x01, 0x00, 0x03});
  uint16_t pair[2] = {1, 2};
  result = master.writeMultipleHoldingRegisters(7, 1, pair, 2);
  emit("unexpected_quantity", result);

  reset();
  result = master.readCoils(0, 0, coils, 1);
  emit("invalid_read_id", result);

  reset();
  result = master.readCoils(1, 0, nullptr, 1);
  emit("invalid_read_buffer", result);

  reset();
  result = master.readCoils(1, 0, coils, 0);
  emit("invalid_read_quantity", result);

  reset();
  result = master.writeMultipleHoldingRegisters(248, 0, pair, 2);
  emit("invalid_write_id", result);

  reset();
  result = master.writeMultipleHoldingRegisters(1, 0, nullptr, 2);
  emit("invalid_write_buffer", result);

  reset();
  result = master.writeMultipleHoldingRegisters(1, 0, pair, 124);
  emit("invalid_write_quantity", result);

  reset();
  result = master.fc69WriteSingleCoil(0, 0, true);
  emit("invalid_fc69_target", result);

  reset();
  result = master.fc69WriteRaw(7, 3, rawPdu, 4);
  emit("invalid_fc69_inner_fc", result);

  reset();
  result = master.fc69WriteRaw(7, 5, nullptr, 1);
  emit("invalid_fc69_buffer", result);

  reset();
  result = master.fc69WriteRaw(7, 5, rawPdu, 3);
  emit("invalid_fc69_single_length", result);

  reset();
  uint8_t oversizedPdu[255] = {};
  result = master.fc69WriteRaw(7, 5, oversizedPdu, 255);
  emit("invalid_fc69_length_overflow", result);

  reset();
  const uint8_t malformedCoils[6] = {0, 0, 0, 9, 1, 0};
  result = master.fc69WriteRaw(7, 15, malformedCoils,
                               sizeof(malformedCoils));
  emit("invalid_fc69_coil_shape", result);

  reset();
  result = master.fc69WriteMultipleCoils(7, 0, multiCoils, 1953);
  emit("invalid_fc69_coil_quantity", result);

  reset();
  result = master.fc69WriteMultipleHoldingRegisters(7, 0, pair, 123);
  emit("invalid_fc69_register_quantity", result);

  reset();
  for (unsigned value = 0; value <= 20; ++value) {
    if (value) std::cout << ',';
    std::cout << modbusRtuMasterErrorToString(
        static_cast<ModbusRTUMasterError>(value));
  }
  std::cout << '\n';

  return 0;
}
