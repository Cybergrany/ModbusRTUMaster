# ModbusRTUMaster

`ModbusRTUMaster` is an Arduino Modbus RTU master/client library. It builds
requests, validates responses, and exposes a small array-based API for coils
and registers. [`ModbusRTUComm`](https://github.com/Cybergrany/ModbusRTUComm)
handles serial framing, RTU timing, receive recovery, and RS485 direction.

This README describes the public API in the current source tree.

## When to use it

Use this library to:

- poll sensors, meters, controllers, or remote I/O over UART or RS485;
- write one or many coils or holding registers;
- send standard unit-zero broadcast writes without waiting for a response;
- send a targeted, no-response write to one compatible device with function
  code `0x45`; or
- protect shared application buffers while keeping locks away from serial I/O.

## What this fork adds

- **Targeted broadcast helpers.** FC `0x45` wraps FC 5, 6, 15, or 16 for one
  selected unit. This is useful when a multidrop controller needs a
  fire-and-forget update without making every unit apply it. See the
  [public header](src/ModbusRTUMaster.h) and
  [targeted-broadcast contract](src/README.md#fc-0x45-targeted-broadcast).
- **Clear transport failures.** Failed writes, timeouts, CRC errors, malformed
  responses, and Modbus exceptions remain distinct at the caller boundary.
  See [`ModbusRTUMasterError`](src/ModbusRTUMaster.h).
- **Safe no-response ordering.** Broadcast calls retain the required quiet
  period before another request can start. One-shot pre- and post-transmit
  gaps are also available when the transport supports them. See
  [transport capabilities](src/README.md#transport-capabilities).
- **Optional caller-buffer locking.** Reads and multi-value writes can use a
  caller-selected lock type. The lock protects only the memory copy and is
  released before serial I/O. See
  [caller-buffer locking](src/README.md#caller-buffer-locking) and
  [`ModbusRTUMasterLock.h`](src/ModbusRTUMasterLock.h).
- **Optional transport diagnostics.** Detailed timing, recovery, and RX-task
  data can be forwarded without adding a second diagnostics model. See the
  [capability table](src/README.md#transport-capabilities).
- **Deterministic regression tests.** Request bytes, CRCs, response parsing,
  error values, call ordering, code size, and stack usage are checked against
  the current public behavior contract. See [the test guide](test/README.md).

## Quick start

Configure the serial port first. `ModbusRTUMaster::begin()` configures Modbus
timing and direction pins; it does not call `Serial.begin()`.

```cpp
#include <Arduino.h>
#include <ModbusRTUMaster.h>

constexpr unsigned long kBaud = 115200UL;
constexpr uint8_t kUnitId = 7;
constexpr int8_t kDriverEnablePin = 2;

ModbusRTUMaster master(Serial1, kDriverEnablePin);
uint16_t inputValues[4] = {};

void setup() {
  Serial1.begin(kBaud, SERIAL_8N1);
  master.begin(kBaud, SERIAL_8N1);
  master.setTimeout(250UL);
}

void loop() {
  const ModbusRTUMasterError result =
      master.readInputRegisters(kUnitId, 0, inputValues, 4);

  if (result == MODBUS_RTU_MASTER_EXCEPTION_RESPONSE) {
    const uint8_t exceptionCode = master.getExceptionResponse();
    (void)exceptionCode;
  } else if (result != MODBUS_RTU_MASTER_SUCCESS) {
    const char* message = modbusRtuMasterErrorToString(result);
    (void)message;
  }

  delay(100);
}
```

The serial object, baud rate, format, and transceiver pins must match the
connected bus. An RE pin may be passed as the third constructor argument when
the transceiver exposes it separately.

## Supported operations

| Function | Method |
| --- | --- |
| FC 1: Read Coils | `readCoils()` |
| FC 2: Read Discrete Inputs | `readDiscreteInputs()` |
| FC 3: Read Holding Registers | `readHoldingRegisters()` |
| FC 4: Read Input Registers | `readInputRegisters()` |
| FC 5: Write Single Coil | `writeSingleCoil()` |
| FC 6: Write Single Holding Register | `writeSingleHoldingRegister()` |
| FC 15: Write Multiple Coils | `writeMultipleCoils()` |
| FC 16: Write Multiple Holding Registers | `writeMultipleHoldingRegisters()` |

Standard write methods accept unit ID `0` for a Modbus broadcast. Broadcasts
do not return a device acknowledgement.

## Targeted broadcast

FC `0x45` is an extension and must be supported by the receiving device. The
outer frame uses unit ID zero, while the target unit and standard write
function are encoded in the data:

```cpp
const uint16_t setpoints[] = {1200, 1250};
const ModbusRTUMasterError result =
    master.fc69WriteMultipleHoldingRegisters(
        7,       // target unit
        100,     // first holding register
        setpoints,
        2);
```

A success result means the frame was accepted by the transport. It does not
prove that the target received or applied the write. Use a later unicast read
when confirmation is required.

## Shared buffers and extra gaps

The default lock is a no-op. Applications that share buffers between tasks can
select a lock type at compile time and pass its pointer to read and multi-write
calls. The selection must be the same in every translation unit. Full setup
and ordering examples are in [the API guide](src/README.md).

`setPreTxGapUsOnce()` and `setPostTxGapUsOnce()` apply an extra delay to the
next transaction only. They are useful for devices that need more recovery
time than the base RTU interval without slowing every request.

## Examples and reference

- [Basic master example](examples/ModbusRTUMasterExample/ModbusRTUMasterExample.ino)
- [Interactive bus probe](examples/ModbusRTUMasterProbe/ModbusRTUMasterProbe.ino)
- [Detailed API and ownership guide](src/README.md)
- [Public header](src/ModbusRTUMaster.h)
- [Transport library](https://github.com/Cybergrany/ModbusRTUComm)
- [Validation guide](test/README.md)

## Testing

Run the native behavior and footprint checks from the repository root:

```sh
test/run_native_tests.sh
```

Native tests cannot prove electrical RS485 timing or target-specific UART
behavior. Validate those on each supported board and transceiver design.

## License

MIT. See [LICENSE](LICENSE).
