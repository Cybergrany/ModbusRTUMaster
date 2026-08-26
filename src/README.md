# Public API and ownership

The supported include is:

```cpp
#include <ModbusRTUMaster.h>
```

`ModbusRTUMaster` builds requests, validates responses, and copies values to
or from caller-owned arrays. `ModbusRTUComm` owns RTU timing, serial framing,
receive recovery, and RS485 direction control. The application owns the serial
port, polling schedule, retries, and register meaning.

## Standard operations

The class supports FC 1, 2, 3, 4, 5, 6, 15, and 16. Unit ID zero is valid only
for write operations and is fire-and-forget.

```cpp
#include <ModbusRTUMaster.h>

ModbusRTUMaster master(Serial1, 2);  // Stream, DE pin; RE is optional
uint16_t registers[4] = {};

void setup() {
  Serial1.begin(115200, SERIAL_8N1);
  master.begin(115200, SERIAL_8N1);
  master.setTimeout(250);
}

void pollDevice() {
  const ModbusRTUMasterError result =
      master.readHoldingRegisters(7, 0, registers, 4);
  if (result == MODBUS_RTU_MASTER_EXCEPTION_RESPONSE) {
    const uint8_t exception = master.getExceptionResponse();
    (void)exception;
  }
}
```

The constructor default response-start timeout is 500 milliseconds.
`setTimeout()` changes that timeout; it does not change T1.5, T3.5, or serial
drain timing.

A failed transport write maps to `MODBUS_RTU_MASTER_FRAME_ERROR` before any
response read. Timeout, frame, CRC, response-shape, ID, function, and exception
errors remain distinct. `modbusRtuMasterErrorToString()` provides a short
constant string for logging.

## FC `0x45` targeted broadcast

A targeted broadcast keeps the outer unit ID at zero. Its first data byte is
the target unit and its second data byte is inner FC 5, 6, 15, or 16.

```cpp
const uint16_t values[] = {0x1122, 0x3344};
const ModbusRTUMasterError result =
    master.fc69WriteMultipleHoldingRegisters(
        7,       // target unit encoded inside the broadcast
        0x1234,  // first holding-register address
        values,
        2);
```

That call emits:

```text
00 45 07 10 12 34 00 02 04 11 22 33 44 82 A9
```

No response is read. A successful call means the transport accepted the
frame, not that the target applied it. Use a later unicast read when delivery
must be confirmed.

`fc69WriteRaw()` accepts an already encoded inner PDU. It still validates the
inner function, single-write length, multiple-write quantity and byte count,
and maximum ADU length before sending.

## Caller-buffer locking

Read operations copy a validated response into the caller's array. Multiple
writes copy the caller's array into a local ADU. An optional non-owning lock
can protect only those copies:

```text
read:        write -> read/validate -> lock -> copy -> unlock
multi-write: lock -> copy -> unlock -> write [-> read]
FC 0x45:     lock -> copy -> unlock -> write; no read
```

The lock is never held over transport I/O.

The default type in `ModbusRTUMasterLock.h` is a no-op. A threaded application
can select another lock type for the whole build:

```cpp
#define MBUS_RTU_MASTER_LOCK_HEADER "my_platform/Mutex.h"
#define MBUS_RTU_MASTER_LOCK_TYPE my_platform::Mutex
#include <ModbusRTUMaster.h>

my_platform::Mutex valuesLock;
uint16_t sharedValues[8] = {};

void pollSafely(ModbusRTUMaster& master) {
  master.readInputRegisters(7, 0, sharedValues, 8, &valuesLock);
}
```

The selected header and type must be identical in every translation unit.
Changing them requires a clean rebuild because they change public method
signatures.

## Transport capabilities

The selected transport must advertise
`MBUS_RTU_COMM_HAS_NO_RESPONSE_GATE == 1`. Standard broadcasts and FC `0x45`
return without a response, so the transport must preserve the post-transmit
quiet period before admitting the next request.

Other methods appear only when the transport advertises the matching feature:

| Transport macro | Master API |
| --- | --- |
| `MBUS_RTU_COMM_HAS_ONE_SHOT_GAPS` | `setPreTxGapUsOnce()`, `setPostTxGapUsOnce()` |
| `MBUS_RTU_COMM_HAS_DEBUG_INFO` with detailed metrics enabled | `debugInfo()` |
| `MBUS_RTU_COMM_HAS_RX_STACK_SNAPSHOT` | `rxThreadStackSnapshot()` |

These are compile-time contracts. They do not probe features at runtime.

## Validation boundary

`test/run_native_tests.sh` checks request bytes and CRCs, parsed values, errors,
broadcast ordering, lock ordering, object-size and stack budgets, and an
optional hot-path budget. Transport tests separately cover serial timing and recovery.
Neither host suite can prove UART interrupt latency or electrical RS485
turnaround on a target board.
