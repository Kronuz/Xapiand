# faultlab-design: journal

State: ready for review
Quest: .campaign/xapiand/quests/faultlab-design.md (the lead's; never edit it)

## Doing

Nothing: design written at cpp-libraries:a2c60c7 (pushed); doc check job 261116-143221-279eaf
passed.

## Next

1. Design review (lens design); Detent's lead answers QD1-QD3; owner rules F9.

## Blocked on

Nothing for the design. F9 (sync strength) waits on the owner's ruling; QD1-QD3 are open for
Detent's lead. Neither blocks review.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/faultlab.md` F7 table ("Crash points \| `power_off_after(n)`, `mutations()`"), F4, F5 ("Detent's four variants"), F6, and the sector tearing, namespace, epochs, media, ENOSPC and short-write rows |
| A2 | `docs/design/faultlab.md` F8 ("virtual void preallocate(std::uint64_t offset, std::uint64_t length);"), F9 ("Sync: one durable `sync` (pending the owner's ruling)") |
| A3 | `docs/design/faultlab.md` F10 ("Size against the 475 lines of test_relay_mirror_crash.cc") |
| A4 | `docs/design/faultlab.md` F11 ("One seeded scenario line"), F12 ("One JSON-line event log"), F13 ("One checker: Xapiand's history.py moves to faultlab"), F14 ("netlab (Linux, root or a user namespace)"), F15 ("What runs where") |
| A5 | `docs/design/faultlab.md` F16 ("callers first, the original deleted last, no shim") and "Migration order" steps 0-10 with sizes |
| A6 | `docs/design/faultlab.md` "Questions for Detent's lead (open)" (QD1, QD2, QD3) |

Doc check: job 261116-143221-279eaf, exit 0 at cpp-libraries:a2c60c7.

## Result

`docs/design/faultlab.md` (cpp-libraries:a2c60c7): 16 decisions F1-F16, migration table, three
Detent questions, first buildable steps. No code moved, no repository created.

## Rulings I made

- Recounted sizes at current heads instead of copying the survey: strata 46 source files (40
  tests, 3 benches, CMake, headers), not ~60; consensus 3 tests, not 4; storage.h 47 `IO::` uses;
  added prospect (19 files) and Detent's second recorder (test_agent_store_crash.cc, 512 lines).
- Migration order deviates from the quest's list: journal moves its tests at step 6 but deletes
  memory_io.h at step 8, after consensus and Detent move, so no pinned consumer breaks (F16).
- `File::preallocate` defaults to growing the size via truncate, not a no-op (the survey's
  proposal), since storage's callers expect the size to grow (F8).
- faultfs gains directories on one device (F4): journal MemoryIO's shared-disk prefix is used by
  Detent, and a barrier is device-wide.
- F5's crash policy only fixes coins `crash` already flips, so every pick is a contract-legal
  state and default seeds keep their states.
