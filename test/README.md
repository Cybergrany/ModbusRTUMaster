# ModbusRTUMaster tests

Run the deterministic native gate from the repository root:

```sh
test/run_native_tests.sh
```

The suite runs the current implementation against deterministic ADU,
transport, clock, and lock doubles. Its output must match the checked-in public
behavior trace.

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

The default run also enforces optimized object and stack budgets. The defaults
are the current public baseline: 8,435 object bytes and 400 stack bytes with
the test toolchain. `MBUS_RTU_MASTER_MAX_OBJECT_BYTES` and
`MBUS_RTU_MASTER_MAX_STACK_BYTES` allow a CI target to select tighter or
toolchain-specific limits.

## Optional timing signal

```sh
MBUS_STRICT_PERFORMANCE=1 test/run_native_tests.sh
```

This runs seven hot-path samples and checks the median against
`MBUS_RTU_MASTER_MAX_BENCHMARK_US` (default `40000`). Host timing is noisy, so
the exact event count plus object and stack budgets remain the stronger
deterministic guards. The benchmark warms the same path before each sample and
pins one available CPU when `taskset` is present. Set
`MBUS_RTU_MASTER_PERF_CPU` to select a particular allowed CPU.

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

`fixtures/master_behavior.trace` is the reviewed public behavior contract.
Change it only with an intentional API or wire-behavior update. The transport
and ADU doubles define the master-layer boundary; UART drain, DE/RE edges,
receive recovery, and electrical bus behavior belong to transport and hardware
validation.
