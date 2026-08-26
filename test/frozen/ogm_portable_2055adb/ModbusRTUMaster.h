// Includes code derived from CMB27/ModbusRTUMaster (MIT).
// See THIRD_PARTY_NOTICES.md and LICENSES/CMB27-ModbusRTUMaster-MIT.txt.
#ifndef ModbusRTUMaster_h
#define ModbusRTUMaster_h

#include "Arduino.h"
#include "ModbusADU.h"
#include "ModbusRTUComm.h"
#include "ModbusRTUMasterLock.h"

// Targeted broadcast (FC 0x45)
// ----------------------------
// The master can address a specific slave using Modbus function 0x45 while
// keeping unitId at 0.  Data[0] contains the target slave id and data[1]
// carries the inner function code (0x05, 0x06, 0x0F or 0x10).  The remaining
// bytes hold the inner PDU exactly as a normal unicast write.  No response is
// expected and the communication layer enforces a short post-transmit silence
// so the bus remains idle long enough for slaves to process the frame.
// Applications may later verify state with a normal unicast read if desired.

static const uint8_t  FC_TARGETED_BROADCAST = 0x45;
static const uint16_t FC69_MAX_REGS  = 122;
static const uint16_t FC69_MAX_COILS = 1952;

enum ModbusRTUMasterError : uint8_t {
  MODBUS_RTU_MASTER_SUCCESS = 0,
  MODBUS_RTU_MASTER_INVALID_ID = 1,
  MODBUS_RTU_MASTER_INVALID_BUFFER = 2,
  MODBUS_RTU_MASTER_INVALID_QUANTITY = 3,
  MODBUS_RTU_MASTER_RESPONSE_TIMEOUT = 4,
  MODBUS_RTU_MASTER_FRAME_ERROR = 5,
  MODBUS_RTU_MASTER_CRC_ERROR = 6,
  MODBUS_RTU_MASTER_UNKNOWN_COMM_ERROR = 7,
  MODBUS_RTU_MASTER_UNEXPECTED_ID = 8,
  MODBUS_RTU_MASTER_EXCEPTION_RESPONSE = 9,
  MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE = 10,
  MODBUS_RTU_MASTER_UNEXPECTED_LENGTH = 11,
  MODBUS_RTU_MASTER_UNEXPECTED_BYTE_COUNT = 12,
  MODBUS_RTU_MASTER_UNEXPECTED_ADDRESS = 13,
  MODBUS_RTU_MASTER_UNEXPECTED_VALUE = 14,
  MODBUS_RTU_MASTER_UNEXPECTED_QUANTITY = 15,
  MODBUS_RTU_MASTER_INVALID_TARGET_ID = 16,
  MODBUS_RTU_MASTER_FC69_INVALID_INNER_FC = 17,
  MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY = 18,
  MODBUS_RTU_MASTER_FC69_LENGTH_OVERFLOW = 19
};

// Lightweight error string helper for logs (const data only).
const char* modbusRtuMasterErrorToString(ModbusRTUMasterError err);

class ModbusRTUMaster {
  public:
    ModbusRTUMaster(Stream& serial, int8_t dePin = -1, int8_t rePin = -1);
    void setTimeout(unsigned long timeout);
    void begin(unsigned long baud, uint32_t config = SERIAL_8N1);
    // One-shot pre-transmit gap (in microseconds) for the next transaction.
    void setPreTxGapUsOnce(uint32_t extraGapUs);
    // One-shot post-transmit gap (in microseconds) after the next transaction.
    void setPostTxGapUsOnce(uint32_t extraGapUs);
#if defined(MBUS_DETAILED_METRICS) && MBUS_DETAILED_METRICS
    const ModbusRTUComm::DebugInfo& debugInfo() const { return _rtuComm.debugInfo(); }
#endif
#if MBUS_RTU_RX_THREAD_STACK_WATERMARK_ACTIVE
    ModbusRTUComm::RxThreadStackSnapshot rxThreadStackSnapshot() const {
      return _rtuComm.rxThreadStackSnapshot();
    }
#endif

    ModbusRTUMasterError readCoils(uint8_t id, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError readDiscreteInputs(uint8_t id, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError readHoldingRegisters(uint8_t id, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError readInputRegisters(uint8_t id, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);

    ModbusRTUMasterError writeSingleCoil(uint8_t id, uint16_t address, bool value);
    ModbusRTUMasterError writeSingleHoldingRegister(uint8_t id, uint16_t address, uint16_t value);
    ModbusRTUMasterError writeMultipleCoils(uint8_t id, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError writeMultipleHoldingRegisters(uint8_t id, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);

    // FC69 targeted broadcast helpers
    ModbusRTUMasterError fc69WriteSingleCoil(uint8_t targetId, uint16_t address, bool value);
    ModbusRTUMasterError fc69WriteSingleHoldingRegister(uint8_t targetId, uint16_t address, uint16_t value);
    ModbusRTUMasterError fc69WriteMultipleCoils(uint8_t targetId, uint16_t startAddress, const bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError fc69WriteMultipleHoldingRegisters(uint8_t targetId, uint16_t startAddress, const uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError fc69WriteRaw(uint8_t targetId, uint8_t innerFc, const uint8_t* innerPdu, uint8_t innerLen, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);

    uint8_t getExceptionResponse();

  private:
    ModbusRTUComm _rtuComm;
    uint8_t _exceptionResponse = 0;
    ModbusRTUMasterError _readValues(uint8_t id, uint8_t functionCode, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError _readValues(uint8_t id, uint8_t functionCode, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut = nullptr);
    ModbusRTUMasterError _writeSingleValue(uint8_t id, uint8_t functionCode, uint16_t address, uint16_t value);

    ModbusRTUMasterError _translateCommError(ModbusRTUCommError commError);

};

#endif
