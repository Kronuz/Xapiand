# faultlab-plain-sync review: correctness, follow-up (Linux settle)

Commit: faultlab:83fbde73c008 (landed), compared with faultlab:5d4df9b04d70 (main before the quest)
Date: 2026-11-24
Question: Detent's lead (msg-330). Does a full file sync that settles other files' staged bytes
apply only when sync_platform is macOS, and does a Linux sweep lose states because of it?

This file extends `faultlab-plain-sync-correctness.md` without editing its verdict.

## Answer

The settle is not macOS-only. `File::sync` and `sync_data` call `settle_staged()` whenever
`durable_at(site)` is true, which on Linux is always (fsync, fdatasync). Every state it removes
is one no Linux backend can produce. Nearly all of it predates the quest.

1. A file can hold `staged` bytes on Linux in the model only. faultfs stages at every ordering
   point on every platform, because io.h's contract promises an ordering point only orders. A real
   Linux PosixIO has no ordering-only barrier: `order_files_and_directory` falls back to
   `sync_files_and_directory`, fsync of each file and of the directory. io.h (completion d8b538c,
   lines 291-296) says "A backend without an ordering-only barrier makes them durable here". So on
   Linux the staged bytes are already durable when the point returns, and the model, which can
   still lose them (L1, L5), is stricter than the device.
2. When another file's sync settles those bytes (L3, L4, L6), the model drops only the outcome
   "staged bytes lost". On Linux that outcome cannot occur: the bytes were fsync'd at the point.
   Detent's premise ("one file's sync persists only that file") holds for page-cache bytes. The
   bytes settled here are not in the page cache, since the point's own fsync already persisted
   them. Linux sweeps lose no reachable state.
3. On the contract, a stricter reading than the device: io.h says staged bytes become durable at
   "the next sync_directory()", not at another file's sync. Settling them at a file sync is
   therefore kinder than io.h on every platform. That kindness is pre-existing: main settles them
   whenever an ordering point is open (L3, L4 identical at 5d4df9b), and main's suite asserts it
   ("the barrier's second phase: a file synced after the point makes the staged bytes durable").
   It would matter only for a backend whose ordering point is barrier-only and whose file sync
   flushes the device. completion's PosixIO has no such mode on any platform. macOS plain, the one
   barrier-only mode, now settles nothing at a plain sync.

## Evidence

Job 261124-092756-fa00e2 (exit 0) ran `.scratch/xapiand/review-faultlab-plain-sync-correctness/linux_settle.sh`
and `linux_settle_probe.cc`, built at 83fbde7 with `set_flush_mode(plain|full, linux_kernel)` and
at main 5d4df9b (no flush mode: every sync persists, as on Linux). The table lists the crash
outcomes of `a` (durable `o`, staged `A`) over 256 seeds. The plain and full columns are
identical.

| Case | main 5d4df9b | 83fbde7 linux plain / full |
|---|---|---|
| L1 point staged a; crash | A o | A o |
| L2 point staged a; later create | A A+later o | A A+later o |
| L3 point staged a; b->sync() | A | A |
| L4 point staged a; b->sync_data() | A | A |
| L5 race stray a; crash | A o | A o |
| L6 race stray a; b->sync() | **A o** | **A** |
| L7 race stray a; write X, point, later (F1) | A A+later A0 A0+later AX AX+later o | same as main |

- Pre-existing (main): every state a settle removes when an ordering point is open (L3, L4).
- Introduced (eefba53, R4): L6 alone. A race stray, a file staged after another thread's sync
  settled its point, is settled by a later file sync when no point is open, which removes `o`.
  On Linux that `A` was fsync'd at its own point, so `o` is unreachable there too.
- L7 matches main at 83fbde7, so F1 (the blocking finding at eefba53) is fixed: `oX` and `o0`
  are gone.

## Options (owner's call; none blocking)

1. Keep it (recommended). The model matches every PosixIO backend, and the extra strictness on
   Linux is already the staged-not-durable state at the point itself.
2. Settle at a file sync only on macOS full, the one call that flushes a device cache it did not
   write. Linux sweeps would then keep L3, L4 and L6's `o`, a state Linux cannot produce. That
   gives more false alarms and changes main's tested second-phase behavior for every existing
   user.
