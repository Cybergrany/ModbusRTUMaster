// Includes code derived from CMB27/ModbusRTUMaster (MIT).
// See THIRD_PARTY_NOTICES.md and LICENSES/CMB27-ModbusRTUMaster-MIT.txt.
#include "ModbusRTUMaster.h"
#include <string.h>

// Targeted broadcast (FC 0x45)
// ----------------------------
// Outer frame uses function code 0x45 with unitId 0.  The first byte of the
// data payload selects the target slave id and the second byte contains the
// inner write function code.  Remaining bytes mirror the standard write PDUs.
// Frames are fire-and-forget and no reply is read.  The communication layer
// enforces a brief silent window after each broadcast so the bus stays idle
// long enough for slaves to commit the write.  Applications may follow up with
// a normal unicast read to verify state if required.

const char* modbusRtuMasterErrorToString(ModbusRTUMasterError err){
  switch(err){
    case MODBUS_RTU_MASTER_SUCCESS: return "SUCCESS";
    case MODBUS_RTU_MASTER_INVALID_ID: return "INVALID_ID";
    case MODBUS_RTU_MASTER_INVALID_BUFFER: return "INVALID_BUFFER";
    case MODBUS_RTU_MASTER_INVALID_QUANTITY: return "INVALID_QUANTITY";
    case MODBUS_RTU_MASTER_RESPONSE_TIMEOUT: return "RESPONSE_TIMEOUT";
    case MODBUS_RTU_MASTER_FRAME_ERROR: return "FRAME_ERROR";
    case MODBUS_RTU_MASTER_CRC_ERROR: return "CRC_ERROR";
    case MODBUS_RTU_MASTER_UNKNOWN_COMM_ERROR: return "UNKNOWN_COMM_ERROR";
    case MODBUS_RTU_MASTER_UNEXPECTED_ID: return "UNEXPECTED_ID";
    case MODBUS_RTU_MASTER_EXCEPTION_RESPONSE: return "EXCEPTION_RESPONSE";
    case MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE: return "UNEXPECTED_FUNCTION_CODE";
    case MODBUS_RTU_MASTER_UNEXPECTED_LENGTH: return "UNEXPECTED_LENGTH";
    case MODBUS_RTU_MASTER_UNEXPECTED_BYTE_COUNT: return "UNEXPECTED_BYTE_COUNT";
    case MODBUS_RTU_MASTER_UNEXPECTED_ADDRESS: return "UNEXPECTED_ADDRESS";
    case MODBUS_RTU_MASTER_UNEXPECTED_VALUE: return "UNEXPECTED_VALUE";
    case MODBUS_RTU_MASTER_UNEXPECTED_QUANTITY: return "UNEXPECTED_QUANTITY";
    case MODBUS_RTU_MASTER_INVALID_TARGET_ID: return "INVALID_TARGET_ID";
    case MODBUS_RTU_MASTER_FC69_INVALID_INNER_FC: return "FC69_INVALID_INNER_FC";
    case MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY: return "FC69_INVALID_QUANTITY";
    case MODBUS_RTU_MASTER_FC69_LENGTH_OVERFLOW: return "FC69_LENGTH_OVERFLOW";
    default: return "UNKNOWN_ERROR";
  }
}

ModbusRTUMaster::ModbusRTUMaster(Stream& serial, int8_t dePin, int8_t rePin) : _rtuComm(serial, dePin, rePin) {
  _rtuComm.setTimeout(500);
}

void ModbusRTUMaster::setTimeout(unsigned long timeout) {
  _rtuComm.setTimeout(timeout);
}

void ModbusRTUMaster::setPreTxGapUsOnce(uint32_t extraGapUs) {
  _rtuComm.setPreTxGapUsOnce(extraGapUs);
}

void ModbusRTUMaster::setPostTxGapUsOnce(uint32_t extraGapUs) {
  _rtuComm.setPostTxGapUsOnce(extraGapUs);
}

void ModbusRTUMaster::begin(unsigned long baud, uint32_t config) {
  _rtuComm.begin(baud, config);
}

ModbusRTUMasterError ModbusRTUMaster::readCoils(uint8_t id, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  return _readValues(id, 1, startAddress, buf, quantity, mut);
}

ModbusRTUMasterError ModbusRTUMaster::readDiscreteInputs(uint8_t id, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  return _readValues(id, 2, startAddress, buf, quantity, mut);
}

ModbusRTUMasterError ModbusRTUMaster::readHoldingRegisters(uint8_t id, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  return _readValues(id, 3, startAddress, buf, quantity, mut);
}

ModbusRTUMasterError ModbusRTUMaster::readInputRegisters(uint8_t id, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  return _readValues(id, 4, startAddress, buf, quantity, mut);
}



ModbusRTUMasterError ModbusRTUMaster::writeSingleCoil(uint8_t id, uint16_t address, bool value) {
  return _writeSingleValue(id, 5, address, ((value) ? 0xFF00 : 0x0000));
}

ModbusRTUMasterError ModbusRTUMaster::writeSingleHoldingRegister(uint8_t id, uint16_t address, uint16_t value) {
  return _writeSingleValue(id, 6, address, value);
}



ModbusRTUMasterError ModbusRTUMaster::writeMultipleCoils(uint8_t id, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  const uint8_t functionCode = 15;
  if (id > 247) return MODBUS_RTU_MASTER_INVALID_ID;
  if (!buf) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (quantity == 0 || quantity > 1968) return MODBUS_RTU_MASTER_INVALID_QUANTITY;
  ModbusADU adu;
  uint16_t byteCount = div8RndUp(quantity);
  adu.setUnitId(id);
  adu.setFunctionCode(functionCode);
  adu.setDataRegister(0, startAddress);
  adu.setDataRegister(2, quantity);
  adu.data[4] = byteCount;
  if(mut) mut->lock();
  for (uint16_t i = 0; i < quantity; i++) {
    bitWrite(adu.data[5 + (i >> 3)], i & 7, buf[i]);
  }
  for (uint16_t i = quantity; i < (byteCount * 8); i++) {
    bitClear(adu.data[5 + (i >> 3)], i & 7);
  }
  if(mut) mut->unlock();
  adu.setDataLen(5 + byteCount);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  if (id == 0) {
    return MODBUS_RTU_MASTER_SUCCESS;
  }
  ModbusRTUCommError commError = _rtuComm.readAdu(adu);
  if (commError) return _translateCommError(commError);
  if (adu.getUnitId() != id) return MODBUS_RTU_MASTER_UNEXPECTED_ID;
  if (adu.getFunctionCode() == (functionCode + 0x80)) {
    _exceptionResponse = adu.data[0];
    return MODBUS_RTU_MASTER_EXCEPTION_RESPONSE;
  }
  if (adu.getFunctionCode() != functionCode) return MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE;
  if (adu.getDataLen() != 4) return MODBUS_RTU_MASTER_UNEXPECTED_LENGTH;
  if (adu.getDataRegister(0) != startAddress) return MODBUS_RTU_MASTER_UNEXPECTED_ADDRESS;
  if (adu.getDataRegister(2) != quantity) return MODBUS_RTU_MASTER_UNEXPECTED_QUANTITY;
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::writeMultipleHoldingRegisters(uint8_t id, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  uint8_t functionCode = 16;
  if (id > 247) return MODBUS_RTU_MASTER_INVALID_ID;
  if (!buf) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (quantity == 0 || quantity > 123) return MODBUS_RTU_MASTER_INVALID_QUANTITY;
  uint16_t byteCount = quantity * 2;
  ModbusADU adu;
  adu.setUnitId(id);
  adu.setFunctionCode(functionCode);
  adu.setDataRegister(0, startAddress);
  adu.setDataRegister(2, quantity);
  adu.data[4] = byteCount;
  if(mut) mut->lock();
  for (uint16_t i = 0; i < quantity; i++) {
    adu.setDataRegister(5 + (i * 2), buf[i]);
  }
  if(mut) mut->unlock();
  adu.setDataLen(5 + byteCount);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  if (id == 0) {
    return MODBUS_RTU_MASTER_SUCCESS;
  }
  ModbusRTUCommError commError = _rtuComm.readAdu(adu);
  if (commError) return _translateCommError(commError);
  if (adu.getUnitId() != id) return MODBUS_RTU_MASTER_UNEXPECTED_ID;
  if (adu.getFunctionCode() == (functionCode + 0x80)) {
    _exceptionResponse = adu.data[0];
    return MODBUS_RTU_MASTER_EXCEPTION_RESPONSE;
  }
  if (adu.getFunctionCode() != functionCode) return MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE;
  if (adu.getDataLen() != 4) return MODBUS_RTU_MASTER_UNEXPECTED_LENGTH;
  if (adu.getDataRegister(0) != startAddress) return MODBUS_RTU_MASTER_UNEXPECTED_ADDRESS;
  if (adu.getDataRegister(2) != quantity) return MODBUS_RTU_MASTER_UNEXPECTED_QUANTITY;
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::fc69WriteSingleCoil(uint8_t targetId, uint16_t address, bool value) {
  if (targetId < 1 || targetId > 247) return MODBUS_RTU_MASTER_INVALID_TARGET_ID;
  ModbusADU adu;
  adu.setUnitId(0);
  adu.setFunctionCode(FC_TARGETED_BROADCAST);
  adu.data[0] = targetId;
  adu.data[1] = 5;
  adu.setDataRegister(2, address);
  adu.setDataRegister(4, value ? 0xFF00 : 0x0000);
  adu.setDataLen(6);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::fc69WriteSingleHoldingRegister(uint8_t targetId, uint16_t address, uint16_t value) {
  if (targetId < 1 || targetId > 247) return MODBUS_RTU_MASTER_INVALID_TARGET_ID;
  ModbusADU adu;
  adu.setUnitId(0);
  adu.setFunctionCode(FC_TARGETED_BROADCAST);
  adu.data[0] = targetId;
  adu.data[1] = 6;
  adu.setDataRegister(2, address);
  adu.setDataRegister(4, value);
  adu.setDataLen(6);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::fc69WriteMultipleCoils(uint8_t targetId, uint16_t startAddress, const bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  if (targetId < 1 || targetId > 247) return MODBUS_RTU_MASTER_INVALID_TARGET_ID;
  if (!buf) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (quantity == 0 || quantity > FC69_MAX_COILS) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
  uint16_t byteCount = div8RndUp(quantity);
  uint16_t innerLen = 5 + byteCount;
  if (innerLen > 251) return MODBUS_RTU_MASTER_FC69_LENGTH_OVERFLOW;
  ModbusADU adu;
  adu.setUnitId(0);
  adu.setFunctionCode(FC_TARGETED_BROADCAST);
  adu.data[0] = targetId;
  adu.data[1] = 15;
  adu.setDataRegister(2, startAddress);
  adu.setDataRegister(4, quantity);
  adu.data[6] = byteCount;
  memset(&adu.data[7], 0, byteCount);
  if(mut) mut->lock();
  for (uint16_t i = 0; i < quantity; i++) {
    bitWrite(adu.data[7 + (i >> 3)], i & 7, buf[i]);
  }
  for (uint16_t i = quantity; i < (byteCount * 8); i++) {
    bitClear(adu.data[7 + (i >> 3)], i & 7);
  }
  if(mut) mut->unlock();
  adu.setDataLen(7 + byteCount);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::fc69WriteMultipleHoldingRegisters(uint8_t targetId, uint16_t startAddress, const uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  if (targetId < 1 || targetId > 247) return MODBUS_RTU_MASTER_INVALID_TARGET_ID;
  if (!buf) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (quantity == 0 || quantity > FC69_MAX_REGS) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
  uint16_t byteCount = quantity * 2;
  uint16_t innerLen = 5 + byteCount;
  if (innerLen > 251) return MODBUS_RTU_MASTER_FC69_LENGTH_OVERFLOW;
  ModbusADU adu;
  adu.setUnitId(0);
  adu.setFunctionCode(FC_TARGETED_BROADCAST);
  adu.data[0] = targetId;
  adu.data[1] = 16;
  adu.setDataRegister(2, startAddress);
  adu.setDataRegister(4, quantity);
  adu.data[6] = byteCount;
  if(mut) mut->lock();
  for (uint16_t i = 0; i < quantity; i++) {
    adu.setDataRegister(7 + (i * 2), buf[i]);
  }
  if(mut) mut->unlock();
  adu.setDataLen(7 + byteCount);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::fc69WriteRaw(uint8_t targetId, uint8_t innerFc, const uint8_t* innerPdu, uint8_t innerLen, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  if (targetId < 1 || targetId > 247) return MODBUS_RTU_MASTER_INVALID_TARGET_ID;
  if (!innerPdu && innerLen) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (innerFc != 5 && innerFc != 6 && innerFc != 15 && innerFc != 16) return MODBUS_RTU_MASTER_FC69_INVALID_INNER_FC;
  if ((uint16_t)3 + innerLen > 253) return MODBUS_RTU_MASTER_FC69_LENGTH_OVERFLOW;

  if (innerFc == 15) {
    if (innerLen < 5) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
    uint16_t qty = (innerPdu[2] << 8) | innerPdu[3];
    uint8_t byteCount = innerPdu[4];
    if (qty == 0 || qty > FC69_MAX_COILS) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
    if (byteCount != div8RndUp(qty)) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
    if (innerLen != uint8_t(5 + byteCount)) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
  } else if (innerFc == 16) {
    if (innerLen < 5) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
    uint16_t qty = (innerPdu[2] << 8) | innerPdu[3];
    uint8_t byteCount = innerPdu[4];
    if (qty == 0 || qty > FC69_MAX_REGS) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
    if (byteCount != qty * 2) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
    if (innerLen != uint8_t(5 + byteCount)) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
  } else {
    if (innerLen != 4) return MODBUS_RTU_MASTER_FC69_INVALID_QUANTITY;
  }

  ModbusADU adu;
  adu.setUnitId(0);
  adu.setFunctionCode(FC_TARGETED_BROADCAST);
  adu.data[0] = targetId;
  adu.data[1] = innerFc;
  if(mut) mut->lock();
  for (uint8_t i = 0; i < innerLen; i++) {
    adu.data[2 + i] = innerPdu[i];
  }
  if(mut) mut->unlock();
  adu.setDataLen(2 + innerLen);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  return MODBUS_RTU_MASTER_SUCCESS;
}



uint8_t ModbusRTUMaster::getExceptionResponse() {
  return _exceptionResponse;
}



ModbusRTUMasterError ModbusRTUMaster::_readValues(uint8_t id, uint8_t functionCode, uint16_t startAddress, bool buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  if (id < 1 || id > 247) return MODBUS_RTU_MASTER_INVALID_ID;
  if (!buf) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (quantity == 0 || quantity > 2000) return MODBUS_RTU_MASTER_INVALID_QUANTITY;
  ModbusADU adu;
  adu.setUnitId(id);
  adu.setFunctionCode(functionCode);
  adu.setDataRegister(0, startAddress);
  adu.setDataRegister(2, quantity);
  adu.setDataLen(4);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  ModbusRTUCommError commError = _rtuComm.readAdu(adu);
  if (commError) return _translateCommError(commError);
  if (adu.getUnitId() != id) return MODBUS_RTU_MASTER_UNEXPECTED_ID;
  if (adu.getFunctionCode() == (functionCode + 0x80)) {
    _exceptionResponse = adu.data[0];
    return MODBUS_RTU_MASTER_EXCEPTION_RESPONSE;
  }
  if (adu.getFunctionCode() != functionCode) return MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE;
  uint16_t byteCount = div8RndUp(quantity);
  if (adu.getDataLen() != (1 + byteCount)) return MODBUS_RTU_MASTER_UNEXPECTED_LENGTH;
  if (adu.data[0] != byteCount) return MODBUS_RTU_MASTER_UNEXPECTED_BYTE_COUNT;
  if(mut) mut->lock();
  for (uint16_t i = 0; i < quantity; i++) {
    buf[i] = bitRead(adu.data[1 + (i >> 3)], i & 7);
  }
  if(mut) mut->unlock();
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::_readValues(uint8_t id, uint8_t functionCode, uint16_t startAddress, uint16_t buf[], uint16_t quantity, MBUS_RTU_MASTER_LOCK_TYPE* mut) {
  if (id < 1 || id > 247) return MODBUS_RTU_MASTER_INVALID_ID;
  if (!buf) return MODBUS_RTU_MASTER_INVALID_BUFFER;
  if (quantity == 0 || quantity > 125) return MODBUS_RTU_MASTER_INVALID_QUANTITY;
  ModbusADU adu;
  adu.setUnitId(id);
  adu.setFunctionCode(functionCode);
  adu.setDataRegister(0, startAddress);
  adu.setDataRegister(2, quantity);
  adu.setDataLen(4);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  ModbusRTUCommError commError = _rtuComm.readAdu(adu);
  if (commError) return _translateCommError(commError);
  if (adu.getUnitId() != id) return MODBUS_RTU_MASTER_UNEXPECTED_ID;
  if (adu.getFunctionCode() == (functionCode + 0x80)) {
    _exceptionResponse = adu.data[0];
    return MODBUS_RTU_MASTER_EXCEPTION_RESPONSE;
  }
  if (adu.getFunctionCode() != functionCode) return MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE;
  uint16_t byteCount = quantity * 2;
  if (adu.getDataLen() != (1 + byteCount)) return MODBUS_RTU_MASTER_UNEXPECTED_LENGTH;
  if (adu.data[0] != byteCount) return MODBUS_RTU_MASTER_UNEXPECTED_BYTE_COUNT;
  if(mut) mut->lock();
  for (uint16_t i = 0; i < quantity; i++) {
    buf[i] = adu.getDataRegister(1 + (i * 2));
  }
  if(mut) mut->unlock();
  return MODBUS_RTU_MASTER_SUCCESS;
}

ModbusRTUMasterError ModbusRTUMaster::_writeSingleValue(uint8_t id, uint8_t functionCode, uint16_t address, uint16_t value) {
  if (id > 247) return MODBUS_RTU_MASTER_INVALID_ID;
  ModbusADU adu;
  adu.setUnitId(id);
  adu.setFunctionCode(functionCode);
  adu.setDataRegister(0, address);
  adu.setDataRegister(2, value);
  adu.setDataLen(4);
  if (!_rtuComm.writeAdu(adu)) return MODBUS_RTU_MASTER_FRAME_ERROR;
  if (id == 0) {
    return MODBUS_RTU_MASTER_SUCCESS;
  }
  ModbusRTUCommError commError = _rtuComm.readAdu(adu);
  if (commError) return _translateCommError(commError);
  if (adu.getUnitId() != id) return MODBUS_RTU_MASTER_UNEXPECTED_ID;
  if (adu.getFunctionCode() == (functionCode + 0x80)) {
    _exceptionResponse = adu.data[0];
    return MODBUS_RTU_MASTER_EXCEPTION_RESPONSE;
  }
  if (adu.getFunctionCode() != functionCode) return MODBUS_RTU_MASTER_UNEXPECTED_FUNCTION_CODE;
  if (adu.getDataLen() != 4) return MODBUS_RTU_MASTER_UNEXPECTED_LENGTH;
  if (adu.getDataRegister(0) != address) return MODBUS_RTU_MASTER_UNEXPECTED_ADDRESS;
  if (adu.getDataRegister(2) != value) return MODBUS_RTU_MASTER_UNEXPECTED_VALUE;
  return MODBUS_RTU_MASTER_SUCCESS;
}



ModbusRTUMasterError ModbusRTUMaster::_translateCommError(ModbusRTUCommError commError) {
  switch (commError) {
    case MODBUS_RTU_COMM_SUCCESS:
      return MODBUS_RTU_MASTER_SUCCESS;
    case MODBUS_RTU_COMM_TIMEOUT:
      return MODBUS_RTU_MASTER_RESPONSE_TIMEOUT;
    case MODBUS_RTU_COMM_FRAME_ERROR:
      return MODBUS_RTU_MASTER_FRAME_ERROR;
    case MODBUS_RTU_COMM_CRC_ERROR:
      return MODBUS_RTU_MASTER_CRC_ERROR;
    default:
      return MODBUS_RTU_MASTER_UNKNOWN_COMM_ERROR;
  }
}
