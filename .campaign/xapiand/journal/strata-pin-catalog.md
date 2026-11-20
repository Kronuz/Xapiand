# strata-pin-catalog: journal

State: working
Quest: .campaign/xapiand/quests/strata-pin-catalog.md (the lead's; never edit it)

## Doing

Committed eef8d64b2: strata pin 397c02d5 -> e3edb426, reproducer from Xapiand:7b89dc9f3. No API
changes affect Xapiand (strata diff touched catalog.hh, index.hh, section_io.hh's unused
chunked_spill, work_pool_executor.hh; no Xapiand use of section_io). Local ASan build of test_pool
under quiet light running; job scripts in ~/.scratch/xapiand/strata-pin-catalog/.

## Next

1. Local test_pool run, then submit A1 (5 runs), A2 (--fails-without, pin reverted), A3 (verify).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
