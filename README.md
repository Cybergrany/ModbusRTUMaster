# ModbusRTUMaster

> [!IMPORTANT]
> This branch is the OpenGameMaster compatibility line, not the current CMB27
> development line. The functional Master replay is implemented and natively
> differential-tested and pins an immutable software-gated Comm candidate. Do
> not use it as an OGM replacement until the exact consumer builds and hardware
> checkpoint are recorded and an immutable compatibility release is tagged.

This repository preserves two intentionally different histories:

| Ref | Purpose | Consumer guidance |
| --- | --- | --- |
| `main` | Mirrors the current `CMB27/ModbusRTUMaster` line without OGM changes. | Use to review or incorporate upstream work, not as an automatic OGM upgrade. |
| `ogm/compat` | Starts at CMB27 `59fe589d2cf389b17b7eeb127254a28de144d6e3`, the source imported by OGM. | OGM changes are replayed here in small, test-gated commits. Consume only an immutable validated tag or commit. |

The exact anchors, matching source hashes and replay rules are recorded in
[OGM_FORK_PROVENANCE.md](OGM_FORK_PROVENANCE.md) and
[`ogm-fork-lock.json`](ogm-fork-lock.json). Do not merge or rebase `main` into
`ogm/compat`: a newer-upstream reconciliation is a separate behavior change.

## Compatibility-line status

The Master protocol replay now includes OGM's FC `0x45` targeted-broadcast
helpers, transmit-admission errors, caller-buffer synchronization, error names
and neutral Comm capability forwarding. It deliberately does not own UART
timing, drain/DE/RE, receive recovery, tasks or diagnostics policy; those live
in the separately reviewed ModbusRTUComm fork.

Check `ogm_functional_replay` in `ogm-fork-lock.json` before treating a commit
as a migration candidate. A tag is not considered a compatibility release
until the validation gates in the provenance document are satisfied.

The current native gate compares 45 deterministic scenarios against the exact
`OGM_Portable` `2055adb` source oracle. Request bytes and CRCs, parsed values,
all error paths and lock/Comm ordering match. Normalized optimized disassembly,
per-function stack usage, symbol sizes and object footprint are also exact. See
[test/README.md](test/README.md) for the commands and the remaining hardware
boundary.

## Installing a validated compatibility release

PlatformIO consumers should pin an immutable compatibility tag or full commit,
never a moving branch:

```ini
lib_deps =
  https://github.com/Cybergrany/ModbusRTUMaster.git#<validated-tag-or-40-char-commit>
```

The `library.json` at that pinned revision also pins the corresponding
`Cybergrany/ModbusRTUComm` commit and the reviewed CMB27 `ModbusADU` commit.
Do not override those dependencies independently unless the complete tuple has
been revalidated. The seed commit is suitable for provenance inspection and
compile experiments only; it is not the eventual OGM dependency pin.

For local extraction work, use a path dependency so the consumer and library
changes are tested together:

```ini
lib_deps =
  symlink:///absolute/path/to/ModbusRTUMaster
```

Before publishing a release, replace the local path with the immutable remote
revision and rebuild the exact OGM dependency tree from a clean dependency
cache.

## Compatibility usage example

The historical compatibility API returns `ModbusRTUMasterError`, rather than a
`bool`. It also expects the application to configure the serial port; the
library's `begin()` computes RTU timing and initializes direction pins but does
not call `HardwareSerial::begin()` on this seed.

```cpp
#include <Arduino.h>
#include <ModbusRTUMaster.h>

namespace {
constexpr unsigned long kBaud = 115200UL;
constexpr int8_t kDriverEnablePin = 2;
ModbusRTUMaster modbus(Serial1, kDriverEnablePin);
}

void setup() {
  Serial1.begin(kBaud, SERIAL_8N1); // configure the UART first
  modbus.begin(kBaud, SERIAL_8N1);  // derive RTU gaps and initialize DE/RE
  modbus.setTimeout(500UL);
}

void loop() {
  uint16_t holding[4] = {};
  const ModbusRTUMasterError result =
      modbus.readHoldingRegisters(7, 0, holding, 4);

  if (result == MODBUS_RTU_MASTER_EXCEPTION_RESPONSE) {
    const uint8_t exceptionCode = modbus.getExceptionResponse();
    (void)exceptionCode;
  }
}
```

Targeted broadcast is exposed as explicit FC `0x45` helpers. It is
fire-and-forget: success means Comm transmitted/admitted the frame, not that a
slave acknowledged it.

```cpp
const uint16_t page[] = {0x1122, 0x3344};
const ModbusRTUMasterError sent =
    modbus.fc69WriteMultipleHoldingRegisters(7, 0x1234, page, 2);
```

Applications that pass a caller-buffer lock select a platform type at compile
time. This keeps platform and RTOS headers out of the library while retaining
direct, allocation-free `lock()`/`unlock()` calls:

```ini
build_flags =
  -DMBUS_RTU_MASTER_LOCK_HEADER=\"platform/PlatformMutex.h\"
  -DMBUS_RTU_MASTER_LOCK_TYPE=PlatformMutex
```

The lock covers only the caller-buffer copy. It is released before serial I/O;
it does not serialize complete Modbus transactions. See [src/README.md](src/README.md)
for the complete public contract and usage examples.

## OGM compatibility contract

Moving source ownership into this repository must not change:

- serialized request bytes, CRCs, response parsing or exception values;
- request order, including FC 0x45 fire-and-forget/no-response ordering;
- UART drain, driver-enable and receive-enable transitions or settling delays;
- timeout boundaries, retry behavior, lock spans or caller-visible errors;
- stack/RAM/flash footprint and hot-path performance beyond the accepted gates;
- compatibility with the deployed original OGM slave firmware.

Native fixtures and byte comparisons can strongly constrain those properties,
but they cannot prove UART interrupt latency, RS485 electrical timing, scheduler
behavior or bus contention. No Stage C bridge or hardware-validation claim is
made by this seed documentation.

## Historical upstream documentation

The reference below is retained from the CMB27 branch point. It contains known
documentation drift from the compatibility header (for example, some methods
are described as returning `bool`). For compatibility work, the pinned source
and the example above are authoritative until the replayed API is documented
and tagged.

> [!WARNING]  
> ModbusRTUMaster is currently undergoing a redesign.
> Things may not work, or may not be corretly documented.
> For a functional version of this library, please use the latest tagged release.

Modbus is an industrial communication protocol. The RTU variant communicates over serial lines such as UART, RS-232, or RS-485. The full details of the Modbus protocol can be found at [modbus.org](https://modbus.org). A good summary can also be found on [Wikipedia](https://en.wikipedia.org/wiki/Modbus).  

This is an Arduino library that implements the master/client logic of the Modbus RTU protocol. This library implements function codes 1 (Read Coils), 2 (Read Discrete Inputs), 3 (Read Holding Registers), 4 (Read Input Registers), 5 (Write Single Coil), 6 (Write Single Holding Register), 15 (Write Multiple Coils), and 16 (Write Multiple Holding Registers).  

This library will work with HardwareSerial, SoftwareSerial, or Serial_ (USB Serial on ATmega32u4 based boards). A driver enable pin can be set up, enabling an RS-485 transceiver to be used. Only `SERIAL_8N1`, `SERIAL_8E1`, `SERIAL_8O1`, `SERIAL_8N2`, `SERIAL_8E2`, and `SERIAL_8O2` are supported when using HardwareSerial; attempting to use any other configuration will cause the library to default to `SERIAL_8N1`.  

## Examples
- [ModbusRTUMasterExample](https://github.com/CMB27/ModbusRTUMaster/blob/main/examples/ModbusRTUMasterExample/ModbusRTUMasterExample.ino)
- [ModbusRTUMasterProbe](https://github.com/CMB27/ModbusRTUMaster/blob/main/examples/ModbusRTUMasterProbe/ModbusRTUMasterProbe.ino)

## Methods

### ModbusRTUMaster()

#### Description
Creates a ModbusRTUMaster object and sets the serial port to use for data transmission.
Optionally sets a driver enable pin. This pin will go `HIGH` when the library is transmitting. This is primarily intended for use with an RS-485 transceiver, but it can also be a handy diagnostic when connected to an LED.

#### Syntax
``` C++
ModbusRTUMaster(serial)
ModbusRTUMaster(serial, dePin)
```

#### Parameters
- `serial`: the serial port object to use for Modbus communication.
- `dePin`: the driver enable pin. This pin is set HIGH when transmitting. If this parameter is set to `NO_DE_PIN`, this feature will be disabled. Default value is `NO_DE_PIN`. Allowed data types `uint8_t` or `byte`.

#### Example
``` C++
# include <ModbusRTUMaster.h>

const uint8_t dePin = 13;

ModbusRTUMaster modbus(Serial, dePin);
```

---


### setTimeout()

#### Description
Sets the maximum timeout in milliseconds to wait for a response after sending a request. The default is 100 milliseconds.

#### Syntax
``` C++
modbus.setTimeout(time)
```

#### Parameters
- `time`: the timeout duration in milliseconds. Allowed data types: `unsigned long`.

---


### begin()

#### Description
Sets the data rate in bits per second (baud) for serial transmission.
Optionally it also sets the data configuration. Note, there must be 8 data bits for Modbus RTU communication. The default configuration is 8 data bits, no parity, and one stop bit.

#### Syntax
``` C++
modbus.begin(baud)
modbus.begin(baud, config)
modbus.begin(baud, config, rxPin, txPin, invert) // ESP32 only
```

#### Parameters
- `baud`: the baud rate to use for Modbus communication. Common values are: `1200`, `2400`, `4800`, `9600`, `19200`, `38400`, `57600`, and `115200`. Allowed data types: `unsigned long`.
- `config`: the serial port configuration to use. Valid values are:  
`SERIAL_8N1`: no parity (default)  
`SERIAL_8N2`  
`SERIAL_8E1`: even parity  
`SERIAL_8E2`  
`SERIAL_8O1`: odd parity  
`SERIAL_8O2`
- `rxPin`: the pin on which to receive serial data. Allowed data types: `int8_t`, `char`.
- `txPin`: the pin on which to transmit serial data. Allowed data types: `int8_t`, `char`.
- `invert`: will invert RX/TX polarity. Allowed data types: `bool`.

_If using a SoftwareSerial port a configuration of `SERIAL_8N1` will be used regardless of what is entered._  
_The `rxPin`, `txPin`, and `invert` parameters are only available on ESP32 boards._

---


### readCoils()

#### Description
reads coil values from a slave/server device.

#### Syntax
``` C++
modbus.readCoils(slaveId, startAddress, buffer, quantity)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `startAddress`: the address of the first coil to read. Allowed data types: `uint16_t`.
- `buffer`: an array in which to place the read coil values. Allowed data types: array of `bool`.
- `quantity`: the number of coil values to read. This value must not be larger than the size of the array. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

#### Example
``` C++
bool coils[8];
modbus.readCoils(1, 0, coils, 8);
```

---


### readDiscreteInputs()

#### Description
reads discrete input values from a slave/server device.

#### Syntax
``` C++
modbus.readDiscreteInputs(slaveId, startAddress, buffer, quantity)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `startAddress`: the address of the first discrete input to read. Allowed data types: `uint16_t`.
- `buffer`: an array in which to place the read discrete input values. Allowed data types: array of `bool`.
- `quantity`: the number of discrete input values to read. This value must not be larger than the size of the array. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

#### Example
``` C++
bool discreteInputs[8];
modbus.readDiscreteInputs(1, 0, discreteInputs, 8);
```

---


### readHoldingRegisters()

#### Description
reads holding register values from a slave/server device.

#### Syntax
``` C++
modbus.readHoldingRegisters(slaveId, startAddress, buffer, quantity)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `startAddress`: the address of the first holding register to read. Allowed data types: `uint16_t`.
- `buffer`: an array in which to place the read holding register values. Allowed data types: array of `uint16_t`.
- `quantity`: the number of holding register values to read. This value must not be larger than the size of the array. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

#### Example
``` C++
uint16_t holdingRegisters[8];
modbus.readHoldingRegisters(1, 0, holdingRegisters, 8);
```

---


### readInputRegisters()

#### Description
reads input register values from a slave/server device.

#### Syntax
``` C++
modbus.readInputRegisters(slaveId, startAddress, buffer, quantity)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `startAddress`: the address of the first input register to read. Allowed data types: `uint16_t`.
- `buffer`: an array in which to place the read input register values. Allowed data types: array of `uint16_t`.
- `quantity`: the number of input register values to read. This value must not be larger than the size of the array. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

#### Example
``` C++
uint16_t inputRegisters[8];
modbus.readInputRegisters(1, 0, inputRegisters, 8);
```

---


### writeSingleCoil()

#### Description
writes a single coil value to a slave/server device.

#### Syntax
``` C++
modbus.writeSingleCoil(slaveId, address, value)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `address`: the address of the coil to write to. Allowed data types: `uint16_t`.
- `value`: the value to write to the coil. Allowed data types: `bool`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

---


### writeSingleHoldingRegister()

#### Description
writes a single holding register value to a slave/server device.

#### Syntax
``` C++
modbus.writeSingleHoldingRegister(slaveId, address, value)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `address`: the address of the holding register to write to. Allowed data types: `uint16_t`.
- `value`: the value to write to the holding register. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

---


### writeMultipleCoils()

#### Description
writes multiple coil values to a slave/server device.

#### Syntax
``` C++
modbus.writeMultipleCoils(slaveId, startingAddress, buffer, quantity)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `startAddress`: the address of the first coil to write to. Allowed data types: `uint16_t`.
- `buffer`: an array of coil values. Allowed data types: array of `bool`.
- `quantity`: the number of coil values to write. This value must not be larger than the size of the array. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

#### Example
``` C++
bool coils[2] = {true, false};
modbus.writeMultipleCoils(1, 0, coils, 2);
```

---


### writeMultipleHoldingRegisters()

#### Description
writes multiple holding register values to a slave/server device.

#### Syntax
``` C++
modbus.writeMultipleHoldingRegisters(slaveId, startingAddress, buffer, quantity)
```

#### Parameters
- `slaveId`: the id number of the device to send this request to. Allowed data types: `uint8_t` or `byte`.
- `startAddress`: the address of the first holding register to write to. Allowed data types: `uint16_t`.
- `buffer`: an array of holding register values. Allowed data types: array of `uint16_t`.
- `quantity`: the number of holding register values to write. This value must not be larger than the size of the array. Allowed data types: `uint16_t`.

#### Returns
`true` if the request was sent and a valid response was received. `false` otherwise. Data type: `bool`.

#### Example
``` C++
uint16_t holdingRegisters[2] = {42, 328};
modbus.writeMultipleHoldingRegisters(1, 0, holdingRegisters, 2);
```

---


### getTimeoutFlag()

#### Description
Checks whether a timeout has occured since the last time the flag was cleared.
This flag is set whenever a timeout occurs and cleared when the `clearTimeoutFlag()` function is called.

#### Syntax
``` C++
modbus.getTimeoutFlag()
```

#### Parameters
None

#### Returns
The current value of the flag. Data type: `bool`.

---


### clearTimeoutFlag()

#### Description
Clears the timeout flag.

#### Syntax
``` C++
modbus.clearTimeoutFlag()
```

#### Parameters
None

---


### getExceptionResponse()

#### Description
Checks the last exception response that has occured since the `clearExceptionResponse()` function was called.

When a valid Modbus request is sent, and the recipient slave/server device cannot process it, it responds with an exception code.
This response is called an exception resonse, and it can be helpful in diagnosing issues.

#### Syntax
``` C++
modbus.getExceptionResponse()
```

#### Parameters
None

#### Returns
Exception code. Data type: `uint8_t`.

- 0: none
- 1: illegal function
- 2: illegal data address
- 3: illegal data value
- 4: server device failure

_Details on exeption responses can be found in the [MODBUS Application Protocol Specification](https://modbus.org/docs/Modbus_Application_Protocol_V1_1b3.pdf). More exeption responses exist than are listed here, but these are the most common._

---


### clearExceptionResponse()

#### Description
clears the exeption response variable.

#### Syntax
``` C++
modbus.clearExceptionResponse()
```

#### Parameters
None
