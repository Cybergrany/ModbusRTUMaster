#ifndef MODBUS_RTU_MASTER_LOCK_H_
#define MODBUS_RTU_MASTER_LOCK_H_

// Compile-time selection point for the optional caller-buffer lock accepted by
// ModbusRTUMaster's read and multi-write APIs.
//
// The lock is non-owning. When non-null, the master calls lock() immediately
// before copying the caller's buffer and unlock() immediately afterward. It is
// deliberately not held over serial I/O and does not serialize transactions.
//
// A consumer that needs a real lock selects its platform type at compile time:
//
//   -DMBUS_RTU_MASTER_LOCK_HEADER=\"my/Lock.h\"
//   -DMBUS_RTU_MASTER_LOCK_TYPE=my_namespace::MyLock
//
// This direct-call seam adds no virtual dispatch or allocation. The neutral
// default is sufficient for callers that omit the optional lock argument.
#if defined(MBUS_RTU_MASTER_LOCK_HEADER)
#include MBUS_RTU_MASTER_LOCK_HEADER
#endif

#ifndef MBUS_RTU_MASTER_LOCK_TYPE
class ModbusRTUMasterNoopBufferLock {
  public:
    void lock() {}
    void unlock() {}
};
#define MBUS_RTU_MASTER_LOCK_TYPE ModbusRTUMasterNoopBufferLock
#endif

#endif  // MODBUS_RTU_MASTER_LOCK_H_
