#ifndef MODBUS_RTU_MASTER_LOCK_H_
#define MODBUS_RTU_MASTER_LOCK_H_

// Compile-time selection point for the optional caller-buffer lock accepted by
// ModbusRTUMaster's existing read/write APIs.
//
// The lock is non-owning. When non-null, the master calls lock() immediately
// before copying the caller's buffer and unlock() immediately afterward. It is
// not held over serial I/O and does not serialize complete transactions.
//
// The default expands to the existing PlatformMutex type, preserving source and
// ABI behavior for OGM consumers. Another package can provide a lockable type
// without pulling OGM's platform compatibility header into Modbus logic:
//
//   -DMBUS_RTU_MASTER_LOCK_HEADER=\"my/Lock.h\"
//   -DMBUS_RTU_MASTER_LOCK_TYPE=my_namespace::MyLock
#ifndef MBUS_RTU_MASTER_LOCK_HEADER
#define MBUS_RTU_MASTER_LOCK_HEADER "platform/PlatformMutex.h"
#endif

#include MBUS_RTU_MASTER_LOCK_HEADER

#ifndef MBUS_RTU_MASTER_LOCK_TYPE
#define MBUS_RTU_MASTER_LOCK_TYPE PlatformMutex
#endif

#endif  // MODBUS_RTU_MASTER_LOCK_H_
