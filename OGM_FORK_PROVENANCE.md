# OpenGameMaster fork provenance

This repository retains the complete CMB27 Git history. Its maintained `main`
starts at the historical source revision imported by OGM and records reviewed
OGM improvements as ordinary descendant commits. Current CMB27 development is
tracked through the `upstream` remote, not by publishing a competing API on
this fork's default branch.

Do not recreate the maintained line from the latest upstream API. Doing so
would mix a later redesign with the ownership move and invalidate the
no-behaviour-change evidence.

## Immutable anchors

| Purpose | Commit |
| --- | --- |
| CMB27 revision reviewed before fork seeding | `78ed238428411c9fb781a2a7e04ec97a0c80371b` |
| Maintained OGM line branch point | `59fe589d2cf389b17b7eeb127254a28de144d6e3` |
| First OGM import in `OpenGameMaster_pio` | `ef4d0ccb1cccd60781f3605d444a16dc7900661c` |
| Later transfer into `OGM_Portable` | `a3551eab76877052df39a5e3ac875f71ce4e1109` |
| Validated functional replay source | `2055adb449c1e767217f09f99efda32e52a0306d` |

The two files imported by `ef4d0ccb` were compared with CMB27 at the branch
point. Their SHA-256 values match exactly:

| Source | SHA-256 |
| --- | --- |
| `src/ModbusRTUMaster.h` | `bc8112626cdcfe2328cff3bb6d665ea1232305b0f303ab79e6d2155b4d2f14a6` |
| `src/ModbusRTUMaster.cpp` | `6248c81cd20761cd76b597751d8e0b56b764dc0873b050e79b75c98478ea162b` |

The machine-readable form of these anchors and exact package dependency pins
is [`ogm-fork-lock.json`](ogm-fork-lock.json).

## Current release status

The master-layer replay is implemented, natively differential-tested, and
released at immutable tag `2.0.0-ogm.1`. That tag points to exact
behavior-bearing commit `61491e27593f15ad9a10e0fcf74595f623a63c1b`.
`library.json` and `ogm-fork-lock.json` pin ModbusRTUComm
`10477dd4af395331c86692f03f9d0c5c709fa684`; the complete tuple passed clean
consumer builds and scoped physical RS485 checkpoints with unchanged deployed
slave firmware on 2026-08-26. Package manifests retain the unambiguous
OGM-qualified identity and do not reuse upstream's materially different
`2.0.0` release identity.

The replay source is the exact OGM Portable hardware-validated dependency
anchor `2055adb449c1e767217f09f99efda32e52a0306d`. No board definitions, game
state, generated register layout or bridge topology entered this repository.

### Functional replay ledger

| Theme | Source provenance |
| --- | --- |
| FC `0x45` targeted broadcast and Comm-owned no-response gate | `OpenGameMaster_pio` `406a884c`, `ae476e17` |
| Caller-buffer lock spans | `OpenGameMaster_pio` `346f6995`, `e59b26f5`; neutral selector from `OGM_Portable` `8697ed3` |
| Stable error strings | `OpenGameMaster_pio` `a537188b` |
| Failed-transmit error propagation | `OGM_Portable` `41ebf9fb` |
| One-shot pre/post-gap forwarding | `OpenGameMaster_pio` `5918b7e8`; `OGM_Portable` `ba02774a` |
| Metrics and stack snapshot forwarding | `OpenGameMaster_pio` `96591365`; `OGM_Portable` `571f29b2`, `75846947` |

Implementation commit `b6569fcfcaf7e2cb4badeff2d9a39969b750b45b`
contains the neutral replay. Commit
`92625a1de9faae6c7098714a16d13442e3b23370` freezes the exact source oracle,
ADU snapshot and 45-scenario trace gate. Commit
`d45ca427d6ec002c2c99fdb5106cc27b3d97fc03` pins the final Comm tuple and
makes the no-response ordering capability a fail-closed compile contract.

Native validation currently establishes exact candidate/oracle traces for
request bytes, CRCs, response/error mapping, FC `0x45`, standard broadcasts,
locks and completion order. It also rejects Comm revisions that lack the
explicit no-response/next-TX gate. Normalized optimized disassembly,
per-function stack usage, symbols and the 8435-byte host object footprint are
identical; the opt-in paired hot-path benchmark also passes. This supports
running the paired consumer builds against the pinned Comm candidate. It did
not replace the master-only physical RS485 checkpoint; that checkpoint and
the later combined master/bridge checkpoint subsequently passed for the exact
release tuple.

The pinned Master/Comm tuple also links as strict native C++11 and passes the
standalone Arduino compile gate on GIGA/mbed (`102672` flash, `51216` RAM) and
Nano/AVR (`12360` flash, `3205` RAM). The Nano result is compile-only because
the complete transport intentionally exceeds its 2 KiB SRAM.
Whole-firmware resource comparison remains a consumer gate; standalone numbers
must not be compared with an OGM application image as if their link surfaces
were equal.

## Hardware-accepted release evidence

The immutable release tuple is:

```text
ModbusRTUMaster 61491e27593f15ad9a10e0fcf74595f623a63c1b
  -> ModbusRTUComm 10477dd4af395331c86692f03f9d0c5c709fa684
  -> ModbusADU     7cb0e24f0abe86bc83e114325d75fe7a7d878562
```

The exact clean consumer artifacts subsequently flashed by the user were
`MASTER` SHA-256
`cec149eeb0e1751671724cf1d765cbe06e4a55704d1f4662402271f516315eb0`
and `bridge_console` SHA-256
`be0c32c3632c930ae9d8df7bbdf4d63f720e35e77dcf85880851d4239a440b81`.
The master-only log (`master-review.csv`, SHA-256
`0f12caab08b50beb1bb5cba643945a7a1a46d62e6a498b3c43a66ce5c3cd9f6b`)
and the 59.94-minute combined log (`bridge-master.csv`, SHA-256
`53aa85d81f25b6efb907daa1a577e8c7f57dab92f09091d629ea88bd5526cec1`)
showed no release-blocking warning against the established failure, ordering,
timing, reset, queue, hash, ACK-debt, or replay baselines. Gameplay/output feel
was accepted by the user. This evidence does not extend to later
behavior-bearing commits or a different physical topology.

Both maintained public sketches are included in the same exact-tuple gate. The
general example passes GIGA/mbed (`118968` flash, `51728` RAM) and Nano/AVR
(`15352` flash, `3781` RAM); the interactive Probe passes Nano/AVR (`18282`
flash, `3735` RAM). The AVR example results have the same compile-only SRAM
qualification. The pinned Comm candidate independently passes its 39-scenario
native trace, trace-off/C++11 matrices, drain-failure and separate-production-TU
tests, plus GIGA include-shadow and AVR compiles.

## Remote layout

```text
origin    git@github.com:Cybergrany/ModbusRTUMaster.git
upstream  https://github.com/CMB27/ModbusRTUMaster.git
```

The Cybergrany repository is maintained as a GitHub fork of CMB27. Fetch
upstream for comparison or an explicitly test-gated reconciliation; publish
OGM work on this fork's maintained line:

```bash
git fetch upstream --prune --tags
git log --left-right --cherry-pick main...upstream/main
git push origin main
```

Do not overwrite `main` with `upstream/main`. Reconciliation is a source and
behavior migration, not branch housekeeping.

## Replay and release policy

1. Replay OGM master work as small, reviewable themes and record the source OGM
   commits in each commit message.
2. Keep buffer locking, platform services, and diagnostics behind neutral
   library contracts; no OGM pin, board, game, or topology headers belong in
   the public API.
3. Preserve request bytes, response validation, FC 0x45 no-response ordering,
   timing, error values, lock spans, footprint, and performance before changing
   consumers.
4. Pin releases by an immutable tag or commit. Do not consume `main` by a
   moving branch name.
   `library.properties` intentionally omits named Arduino dependencies because
   that format cannot express immutable Git SHAs; use the exact tuple in
   `library.json`/`ogm-fork-lock.json`.
5. Reconcile newer CMB27 changes only in a separate, explicitly test-gated
   change after the maintained package is proven.

## Release gates

A commit or tag is suitable for an OGM consumer only after all of the following
evidence is recorded against the exact dependency tuple:

1. Source provenance: each replay commit names its source OGM commit(s), and
   generated/package locks resolve to immutable dependency commits.
2. Protocol parity: request and response bytes, CRCs, error/exception mapping,
   FC 0x45 no-response behavior and broadcast behavior match the frozen OGM
   fixtures.
3. Ordering and timing parity: serial writes, drain, delays, DE/RE transitions,
   locks, callbacks and message completion occur in the established order.
4. Resource/performance parity: paired hot-path gates pass and embedded
   stack/RAM/flash changes are understood and accepted.
5. Consumer validation: native suites and the exact supported OGM master,
   bridge and legacy-slave firmware builds pass from clean dependency caches.
6. Hardware validation: the release candidate is exercised over the physical
   GIGA/RS485 topology with unchanged deployed slave firmware.

Passing items 1-5 supports a hardware checkpoint because the migration should
not alter on-wire behavior; it does not replace item 6. Conversely, hardware
that merely appears playable does not replace the trace, ordering or
performance gates.

The candidate package pins ModbusADU to
`7cb0e24f0abe86bc83e114325d75fe7a7d878562` and the functional ModbusRTUComm
candidate to `10477dd4af395331c86692f03f9d0c5c709fa684`. Neither dependency may be
advanced independently without rerunning the complete tuple gates.
