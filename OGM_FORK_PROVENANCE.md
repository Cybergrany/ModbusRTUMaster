# OpenGameMaster fork provenance

This repository retains the complete CMB27 Git history. Its two long-lived
lines have deliberately different purposes:

- `main` tracks `upstream/main` without OGM changes.
- `ogm/compat` starts at the historical source revision imported by OGM and is
  the only line on which reviewed OGM compatibility changes may be replayed.

Do not recreate `ogm/compat` from the latest upstream API. Doing so would mix
the later upstream redesign with the repository move and invalidate the
no-behaviour-change migration gate.

## Immutable anchors

| Purpose | Commit |
| --- | --- |
| CMB27 revision reviewed before fork seeding | `78ed238428411c9fb781a2a7e04ec97a0c80371b` |
| `ogm/compat` branch point | `59fe589d2cf389b17b7eeb127254a28de144d6e3` |
| First OGM import in `OpenGameMaster_pio` | `ef4d0ccb1cccd60781f3605d444a16dc7900661c` |
| Later transfer into `OGM_Portable` | `a3551eab76877052df39a5e3ac875f71ce4e1109` |

The two files imported by `ef4d0ccb` were compared with CMB27 at the branch
point. Their SHA-256 values match exactly:

| Source | SHA-256 |
| --- | --- |
| `src/ModbusRTUMaster.h` | `bc8112626cdcfe2328cff3bb6d665ea1232305b0f303ab79e6d2155b4d2f14a6` |
| `src/ModbusRTUMaster.cpp` | `6248c81cd20761cd76b597751d8e0b56b764dc0873b050e79b75c98478ea162b` |

The machine-readable form of these anchors and exact package dependency pins
is [`ogm-fork-lock.json`](ogm-fork-lock.json).

## Remote layout

```text
origin    git@github.com:Cybergrany/ModbusRTUMaster.git
upstream  https://github.com/CMB27/ModbusRTUMaster.git
```

The Cybergrany repository must be created as a GitHub fork of CMB27 before the
local compatibility branch can be pushed. Once it exists:

```bash
git fetch upstream --prune --tags
git push -u origin ogm/compat
```

## Replay and release policy

1. Replay OGM master work as small, reviewable themes and record the source OGM
   commits in each commit message.
2. Keep buffer locking, platform services, and diagnostics behind neutral
   library contracts; no OGM pin, board, game, or topology headers belong in
   the public API.
3. Preserve request bytes, response validation, FC 0x45 no-response ordering,
   timing, error values, lock spans, footprint, and performance before changing
   consumers.
4. Pin releases by an immutable tag or commit. Do not consume `main` or
   `ogm/compat` by a moving branch name.
5. Reconcile newer CMB27 changes only in a separate, explicitly test-gated
   change after the compatibility package is proven.

The seed package pins ModbusADU to
`7cb0e24f0abe86bc83e114325d75fe7a7d878562` and the provenance-only
ModbusRTUComm seed to `d6eca90923aea0a8c0d10716e190f462886205b2`.
The Comm pin must be advanced in a reviewed commit when its functional OGM
transport replay is ready.
