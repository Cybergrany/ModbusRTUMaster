# Compatibility tests

This directory locks the master-layer behavior that OGM was running at
`OGM_Portable` commit
`2055adb449c1e767217f09f99efda32e52a0306d`. The frozen source under
`frozen/ogm_portable_2055adb/` is an oracle, not a second implementation to
maintain. Its hashes are checked before every test run.

Run the deterministic gate from the repository root:

```sh
test/run_native_tests.sh
```

It builds the frozen OGM master and the candidate fork against the same pinned
ADU fixture and deterministic Comm/lock doubles. The resulting traces must be
byte-for-byte identical to each other and to the checked-in golden trace. The
45 scenarios cover:

- FC 1, 2, 3 and 4 request bytes, response decoding and caller-buffer locks;
- FC 5, 6, 15 and 16 unicast and unit-zero broadcast framing;
- FC `0x45` single, multiple and raw targeted-broadcast framing;
- CRC bytes for every emitted request;
- no-response ordering for broadcasts and FC `0x45`;
- transmit-admission failures and Comm error translation;
- exception capture and every response-shape validation error;
- invalid ids, buffers, quantities and FC `0x45` inner-PDU shapes;
- the complete numeric error/string mapping;
- timeout, begin and one-shot pre/post gap forwarding; and
- exact `lock -> unlock -> write` and `write -> read -> lock -> unlock`
  ordering where those spans apply.

The default run also compares optimized symbol sizes and total object footprint
with the frozen OGM implementation. This is a deterministic hot-path/footprint
guard: adding work to the master methods changes the gate even when functional
traces still agree.

For an additional paired host timing signal, run:

```sh
MBUS_STRICT_PERFORMANCE=1 test/run_native_tests.sh
```

The strict mode alternates seven candidate/oracle runs of 250,000 FC `0x45`
multiple-register encodes and compares their medians. It rejects a slowdown
greater than 20%. The wider threshold admits ordinary host scheduling and CPU
frequency noise; exact optimized codegen remains the primary deterministic
performance guard.

## Fixture ownership

- `frozen/ogm_portable_2055adb/` is copied exactly from the validated source
  anchor. Do not format or otherwise modernize it.
- `support/ModbusADU.*` is the exact CMB27 ADU snapshot used by that anchor.
- `support/ModbusRTUComm.h` records the Master/Comm boundary. Transport timing,
  drain, DE/RE and recovery behavior belong to the ModbusRTUComm differential
  suite, not this one.
- `fixtures/ogm_portable_2055adb_master.trace` is regenerated only when OGM
  deliberately adopts a new, independently validated behavior anchor.

The native suite cannot prove UART interrupt latency, RS485 bus idle, physical
driver-enable timing or deployed-slave compatibility. Those remain consumer
build and hardware gates after the exact Master/Comm dependency tuple is pinned.
