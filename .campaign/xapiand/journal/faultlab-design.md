# faultlab-design: journal

State: ready for review
Quest: .campaign/xapiand/quests/faultlab-design.md (the lead's; never edit it)

## Doing

Nothing: both reviews (Xapiand design D1-D4, Detent R1-R7) folded in at cpp-libraries:bcf85b4;
Detent R8/R9 at cpp-libraries:9d23d74 (pushed); doc check job 261116-145731-e88e27 queued at 9d23d74.

## Next

1. Re-review of 9d23d74 (D1-D4, R1, R8); owner rules F9.

## Blocked on

Nothing for the design. F9 (sync) waits on the owner's ruling; it does not block review.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/faultlab.md` F7 ("Public API. One table, everything a test touches"), F5 ("Detent's four variants"), F4, F6; rows for crash points, sector tearing, namespace durability, ordering epochs, media, ENOSPC, short writes |
| A2 | `docs/design/faultlab.md` F8 ("virtual void preallocate(std::uint64_t offset, std::uint64_t length);"), F9 ("Sync: always durable (pending the owner's ruling)") |
| A3 | `docs/design/faultlab.md` F10 ("Size against the 475 lines of test_relay_mirror_crash.cc") |
| A4 | `docs/design/faultlab.md` F11 ("One seeded scenario line"), F12 ("One JSON-line event log"), F13 ("One checker: Xapiand's history.py moves to faultlab"), F14 ("netlab (Linux, root or a user namespace)"), F15 ("What runs where") |
| A5 | `docs/design/faultlab.md` F16 ("callers first, the original deleted last, no shim") and "Migration order" steps 0-10 with sizes |
| A6 | `docs/design/faultlab.md` "Questions for Detent's lead (answered)" (QD1, QD2, QD3) |

Doc check: job 261116-143221-279eaf, exit 0 at a2c60c7; job 261116-144654-56edb5 at bcf85b4.

Review findings, by id (Xapiand review at Xapiand:6853e3469; Detent review faultlab-design-detent.md):

- D1: F3 no longer claims a default superset; opt-in `size_pick::sector` (F5); the review's probe
  (job 261116-143957-91a3bb, sizes.cc) is specified as a faultlab test; steps 5, 6, 9 sweep with it.
- D2: per-field picks; `name_pick::additions` is Entries::All's legal half, both-names states
  dropped as illegal; QD2 follows Detent's answer, with the reproduce-the-old-model job in step 7.
- D3/R1: `IO::link` as the third seam addition (D96), faultfs `link` op drawing no coins when
  unused, `recorder::verify()`, Detent's stores retyped to the seam as step 7's first item,
  faultfs `inspect_entry` and `lock_owner`.
- D4: F9 restated: sync and sync_data always durable; one test or benchmark switch weakens every
  sync; STORAGE_FULL_SYNC maps onto it; `ordered_syncs` dropped; pending the owner.
- R2: placed stops stay in-process; faultlab has no signal code (F15). R3: `isolate` semantics
  named in F14. R4: `kind` lines reserved (F12). R5: kill keeps epochs. R6: recorder takes a
  directory. R7: step 5 recounts at journal-driver. N1, N2, N3, N5 (journal before consensus), N6
  taken. N4, N7, N8, N9 not taken (not in the lead's list).
- R8: faultfs `File::inspect` and `try_lease` (lease table released by kill and crash, `op::lease`)
  in F7; `File::map_shared` as the fourth seam addition, folded into the sector model as a write at
  the next call (F8); ChangeWriter creates, sizes and inspects `change` through the IO and
  `verify(dir, {"change"})` compares it by length (F10). R9: "legal" judged by the IO contract (F5).
- These are design-text fixes: their tests are faultlab's build tests (migration "Checks"), not
  runnable before the repository exists.

## Result

`docs/design/faultlab.md` (cpp-libraries:9d23d74): 16 decisions F1-F16, migration table, three
Detent questions answered, first buildable steps. No code moved, no repository created.

## Rulings I made

- Recounted sizes at current heads instead of copying the survey: strata 46 source files (40
  tests, 3 benches, CMake, headers), not ~60; consensus 3 tests, not 4; storage.h 47 `IO::` uses;
  added prospect (19 files) and Detent's second recorder (test_agent_store_crash.cc, 512 lines).
- journal moves its tests at step 5 (before consensus, per N5) but deletes memory_io.h at step 8,
  after consensus and Detent move, so no pinned consumer breaks (F16).
- `preallocate` on an unsupported filesystem falls back to truncate growth rather than throwing
  not_supported (N3), so storage's tolerance code goes.
- A directory is an entry of its parent's scan, as PosixIO lists it (N2).
- `File::preallocate` defaults to growing the size via truncate, not a no-op (the survey's
  proposal), since storage's callers expect the size to grow (F8).
- faultfs gains directories on one device (F4): journal MemoryIO's shared-disk prefix is used by
  Detent, and a barrier is device-wide.
- F5's crash policy only fixes coins `crash` already flips, so every pick is a contract-legal
  state and default seeds keep their states.
