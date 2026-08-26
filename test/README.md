# ModbusRTUMaster tests

Run the deterministic native gate from the repository root:

```sh
test/run_native_tests.sh
```

The suite runs the current implementation and a frozen reference against the
same deterministic ADU, transport, clock, and lock doubles. Their traces must
match each other and the checked-in golden trace.

## Coverage

The 45 scenarios cover:

- FC 1, 2, 3, and 4 request bytes, response decoding, and buffer locks;
- FC 5, 6, 15, and 16 unicast and unit-zero broadcast framing;
- FC `0x45` single, multiple, and raw targeted broadcasts;
- CRC bytes for every emitted request;
- no-response and next-transmit ordering;
- failed writes and transport error translation;
- exception capture and every response-shape validation error;
- invalid IDs, buffers, quantities, and targeted-broadcast PDU shapes;
- error-to-string mappings;
- timeout, begin, and one-shot gap forwarding; and
- exact lock, write, and read call order.

A negative compile gate rejects a transport that does not advertise the
required no-response gate. Static checks also keep the interactive probe's
serial reconfiguration order consistent.

The default run compares normalized optimized disassembly, relocations, symbol
sizes, total object footprint, and compiler stack-usage records with the
reference implementation.

## Optional timing signal

```sh
MBUS_STRICT_PERFORMANCE=1 test/run_native_tests.sh
```

This alternates candidate and reference samples and compares their median
encode time. Host timing is noisy, so optimized code shape and operation
counts remain the stronger deterministic guards.

## Arduino compile gates

Compile the shipped examples and full public API fixture against a specific
`ModbusRTUComm` checkout:

```sh
PLATFORMIO_CORE_DIR=/absolute/path/to/a/writable/platformio-core \
MBUS_RTU_COMM_SOURCE_DIR=/absolute/path/to/ModbusRTUComm/src \
SCONSFLAGS=-j8 \
test/run_arduino_compile_tests.sh
```

The script verifies the selected transport revision, stages the exact source
tuple, and compiles the normal example, interactive probe, and full API fixture
for the configured AVR and mbed targets.

## Fixture ownership

Frozen source and traces are test oracles, not a second implementation to
maintain. Do not format or modernize them. The transport and ADU doubles define
the master-layer boundary; UART drain, DE/RE edges, receive recovery, and
electrical bus behavior belong to transport and hardware validation.
