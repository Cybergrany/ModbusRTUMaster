#include <Arduino.h>
#include <ModbusRTUMaster.h>

namespace {

ModbusRTUMaster master(Serial, 2, 3);
MBUS_RTU_MASTER_LOCK_TYPE bufferLock;
volatile bool exerciseApi = false;

}  // namespace

void setup() {
  master.begin(115200, SERIAL_8N1);
  master.setTimeout(500);
  master.setPreTxGapUsOnce(100);
  master.setPostTxGapUsOnce(200);

  // Compile and link every public extension without emitting traffic when this
  // fixture is accidentally flashed.
  if (exerciseApi) {
    bool coils[10] = {};
    uint16_t registers[2] = {0x1122, 0x3344};
    uint8_t rawPdu[9] = {
        0x12, 0x34, 0x00, 0x02, 0x04, 0x11, 0x22, 0x33, 0x44};

    (void)master.readCoils(7, 0, coils, 10, &bufferLock);
    (void)master.readHoldingRegisters(7, 0, registers, 2, &bufferLock);
    (void)master.writeMultipleCoils(7, 0, coils, 10, &bufferLock);
    (void)master.writeMultipleHoldingRegisters(
        7, 0, registers, 2, &bufferLock);
    (void)master.fc69WriteSingleCoil(7, 0, true);
    (void)master.fc69WriteSingleHoldingRegister(7, 0, 0x1122);
    (void)master.fc69WriteMultipleCoils(7, 0, coils, 10, &bufferLock);
    (void)master.fc69WriteMultipleHoldingRegisters(
        7, 0, registers, 2, &bufferLock);
    (void)master.fc69WriteRaw(7, 16, rawPdu, sizeof(rawPdu), &bufferLock);
  }
}

void loop() {}
