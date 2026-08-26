#ifndef ModbusRTUComm_h
#define ModbusRTUComm_h

#include "Arduino.h"
#include "ModbusADU.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#define MBUS_RTU_COMM_HAS_ONE_SHOT_GAPS 1
#if !defined(MBUS_TEST_OMIT_NO_RESPONSE_GATE_CAPABILITY)
#define MBUS_RTU_COMM_HAS_NO_RESPONSE_GATE 1
#endif

enum ModbusRTUCommError : uint8_t {
  MODBUS_RTU_COMM_SUCCESS = 0,
  MODBUS_RTU_COMM_TIMEOUT = 1,
  MODBUS_RTU_COMM_FRAME_ERROR = 2,
  MODBUS_RTU_COMM_CRC_ERROR = 3
};

namespace modbus_master_fixture {

enum class Event : uint8_t {
  Begin,
  SetTimeout,
  SetPreTxGap,
  SetPostTxGap,
  Lock,
  Unlock,
  Write,
  Read
};

struct EventRecord {
  Event event;
  uint32_t value;
};

struct State {
  std::vector<EventRecord> events;
  std::vector<uint8_t> tx;
  std::vector<uint8_t> response;
  bool write_result;
  ModbusRTUCommError read_result;
  bool benchmark_mode;
  uint64_t event_count;

  State()
      : write_result(true),
        read_result(MODBUS_RTU_COMM_SUCCESS),
        benchmark_mode(false),
        event_count(0) {}
};

inline State& state() {
  static State value;
  return value;
}

inline void reset() {
  state() = State();
}

inline void record(Event event, uint32_t value = 0) {
  ++state().event_count;
  if (state().benchmark_mode) return;
  state().events.push_back(EventRecord{event, value});
}

inline void queueResponse(uint8_t unitId, uint8_t functionCode,
                          const uint8_t* data, uint16_t dataLen) {
  ModbusADU adu;
  adu.setUnitId(unitId);
  adu.setFunctionCode(functionCode);
  for (uint16_t i = 0; i < dataLen; ++i) {
    adu.data[i] = data[i];
  }
  adu.setDataLen(dataLen);
  adu.updateCrc();
  state().response.assign(adu.rtu, adu.rtu + adu.getRtuLen());
}

inline const char* eventName(Event event) {
  switch (event) {
    case Event::Begin: return "begin";
    case Event::SetTimeout: return "timeout";
    case Event::SetPreTxGap: return "pre_gap";
    case Event::SetPostTxGap: return "post_gap";
    case Event::Lock: return "lock";
    case Event::Unlock: return "unlock";
    case Event::Write: return "write";
    case Event::Read: return "read";
    default: return "unknown";
  }
}

}  // namespace modbus_master_fixture

class ModbusRTUComm {
  public:
    ModbusRTUComm(Stream& serial, int8_t dePin = -1, int8_t rePin = -1)
        : _serial(serial), _dePin(dePin), _rePin(rePin) {}

    void begin(unsigned long baud, uint32_t config = SERIAL_8N1) {
      modbus_master_fixture::record(
          modbus_master_fixture::Event::Begin,
          static_cast<uint32_t>((baud & 0x00FFFFFFUL) |
                                ((config & 0xFFUL) << 24)));
    }

    void setTimeout(unsigned long timeout) {
      modbus_master_fixture::record(modbus_master_fixture::Event::SetTimeout,
                                    static_cast<uint32_t>(timeout));
    }

    void setPreTxGapUsOnce(unsigned long extraGapUs) {
      modbus_master_fixture::record(
          modbus_master_fixture::Event::SetPreTxGap,
          static_cast<uint32_t>(extraGapUs));
    }

    void setPostTxGapUsOnce(unsigned long extraGapUs) {
      modbus_master_fixture::record(
          modbus_master_fixture::Event::SetPostTxGap,
          static_cast<uint32_t>(extraGapUs));
    }

    ModbusRTUCommError readAdu(ModbusADU& adu) {
      using namespace modbus_master_fixture;
      record(Event::Read);
      if (state().read_result != MODBUS_RTU_COMM_SUCCESS) {
        return state().read_result;
      }
      if (state().response.empty()) {
        return MODBUS_RTU_COMM_FRAME_ERROR;
      }
      const size_t length = std::min<size_t>(state().response.size(), 256U);
      for (size_t i = 0; i < length; ++i) {
        adu.rtu[i] = state().response[i];
      }
      adu.setRtuLen(static_cast<uint16_t>(length));
      return MODBUS_RTU_COMM_SUCCESS;
    }

    bool writeAdu(ModbusADU& adu) {
      using namespace modbus_master_fixture;
      adu.updateCrc();
      if (!state().benchmark_mode) {
        state().tx.assign(adu.rtu, adu.rtu + adu.getRtuLen());
      }
      record(Event::Write, adu.getRtuLen());
      return state().write_result;
    }

  private:
    Stream& _serial;
    int8_t _dePin;
    int8_t _rePin;
};

#endif
