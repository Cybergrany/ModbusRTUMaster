# Public Master API and ownership seams

`ModbusRTUMaster` owns request construction and response validation. It has no
knowledge of OGM boards, children, games, generated registers or bridge routes.
It also does not own UART timing, driver-enable sequencing, receive recovery or
threads; those are responsibilities of the pinned `ModbusRTUComm` dependency.

## Standard operations

The class supports FC 1, 2, 3, 4, 5, 6, 15 and 16 with the historical
`ModbusRTUMasterError` return values. Unit id zero remains valid only for write
operations and is fire-and-forget.

Construction sets the response-start timeout to 500 milliseconds, matching the
validated OGM behavior. `setTimeout()` overrides that value in milliseconds;
it does not change RTU inter-character or inter-frame timing.

```cpp
#include <ModbusRTUMaster.h>

ModbusRTUMaster master(Serial1, 2);  // Stream, DE pin; RE is optional

void setup() {
  Serial1.begin(115200, SERIAL_8N1);
  master.begin(115200, SERIAL_8N1);
  master.setTimeout(500);
}

void pollBoard() {
  uint16_t registers[4] = {};
  const ModbusRTUMasterError result =
      master.readHoldingRegisters(7, 0, registers, 4);
  if (result == MODBUS_RTU_MASTER_EXCEPTION_RESPONSE) {
    const uint8_t exception = master.getExceptionResponse();
    (void)exception;
  }
}
```

A failed `ModbusRTUComm::writeAdu()` maps to
`MODBUS_RTU_MASTER_FRAME_ERROR` before any response read. Comm timeout, frame
and CRC errors retain numeric values 4, 5 and 6 at the Master boundary.

## FC `0x45` targeted broadcast

OGM's targeted broadcast keeps outer unit id zero and uses function `0x45`.
The first data byte is the target slave and the second is inner FC 5, 6, 15 or
16. The remaining bytes are the corresponding write PDU.

```cpp
const uint16_t values[] = {0x1122, 0x3344};
const ModbusRTUMasterError result =
    master.fc69WriteMultipleHoldingRegisters(
        7,       // target slave encoded inside the broadcast
        0x1234,  // first holding-register address
        values,
        2);
```

That call emits this complete RTU frame:

```text
00 45 07 10 12 34 00 02 04 11 22 33 44 82 A9
```

No response is read. The Comm layer must advertise and enforce the established
post-transmit/next-message gate. Callers that need confirmation perform a later
normal unicast read; the Master does not infer delivery from successful send.

This ordering guarantee also applies to standard unit-zero writes. The Master
therefore requires `MBUS_RTU_COMM_HAS_NO_RESPONSE_GATE == 1` and deliberately
fails compilation when a selected Comm does not advertise it. A compatible
`writeAdu()` signature alone is insufficient: older Comm revisions would send
the bytes but allow the next request too early.

`fc69WriteRaw()` is intended for already encoded inner PDUs. It still validates
the permitted inner function, exact single-write length, multiple-write
quantity/byte-count agreement and maximum ADU length before transmitting.

## Caller-buffer lock seam

Read operations copy a validated response into the caller's array. Multiple
writes copy the caller's array into a local ADU. An optional non-owning lock can
protect exactly those copies:

```text
read:        write request -> read/validate response -> lock -> copy -> unlock
multi-write: lock -> copy -> unlock -> write request [-> read response]
FC 0x45:     lock -> copy -> unlock -> write request; no response read
```

The lock is never held over Comm I/O. This preserves OGM's established
callback/message-completion ordering and prevents a slow or blocked UART from
extending a caller-buffer critical section.

The default type in `ModbusRTUMasterLock.h` is a neutral no-op type so ordinary
Arduino users acquire no platform dependency or virtual-call overhead. A
consumer needing synchronization supplies a lockable type globally:

```cpp
// Build flags may define these instead. The selected header must declare the
// selected type before ModbusRTUMaster.h is parsed.
#define MBUS_RTU_MASTER_LOCK_HEADER "my_platform/Mutex.h"
#define MBUS_RTU_MASTER_LOCK_TYPE my_platform::Mutex
#include <ModbusRTUMaster.h>

my_platform::Mutex valuesLock;
uint16_t sharedValues[8] = {};

void pollSafely(ModbusRTUMaster& master) {
  master.readInputRegisters(7, 0, sharedValues, 8, &valuesLock);
}
```

The selection is compile-time and must be consistent in every translation unit
that sees the public header. Changing it changes the C++ method signatures and
therefore requires a clean rebuild of the consumer and library.

## Required and optional Comm capabilities

The no-response gate above is mandatory for this compatibility line. Other
forwarding methods are enabled only when the selected Comm header advertises
the matching neutral capability macro:

| Comm macro | Master API |
| --- | --- |
| `MBUS_RTU_COMM_HAS_ONE_SHOT_GAPS` | `setPreTxGapUsOnce()`, `setPostTxGapUsOnce()` |
| `MBUS_RTU_COMM_HAS_DEBUG_INFO` plus enabled detailed metrics | `debugInfo()` |
| `MBUS_RTU_COMM_HAS_RX_STACK_SNAPSHOT` | `rxThreadStackSnapshot()` |

These guards are compile-time contracts, not runtime feature detection. The OGM
compatibility release pins one immutable Comm revision that exposes the same
capabilities as the validated OGM source anchor.

## What tests establish

Run `test/run_native_tests.sh` from the repository root. It differentially
compares the fork with frozen OGM Portable commit `2055adb`, including request
bytes/CRCs, result buffers, error values, no-response paths and event ordering.
It also requires exact normalized optimized disassembly, per-function stack
usage, symbol and object-size parity. The optional
`MBUS_STRICT_PERFORMANCE=1` mode adds a paired hot-path timing check.

Those host gates deliberately stop at the Master/Comm boundary. Physical UART
drain, DE/RE transitions, scheduler latency, RS485 contention and compatibility
with deployed slaves remain Comm/consumer hardware-validation responsibilities.
