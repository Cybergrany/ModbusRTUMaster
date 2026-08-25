#ifndef MODBUS_MASTER_FIXTURE_LOCK_H_
#define MODBUS_MASTER_FIXTURE_LOCK_H_

#include "ModbusRTUComm.h"

class FixtureLock {
  public:
    void lock() {
      modbus_master_fixture::record(modbus_master_fixture::Event::Lock);
    }

    void unlock() {
      modbus_master_fixture::record(modbus_master_fixture::Event::Unlock);
    }
};

#endif
