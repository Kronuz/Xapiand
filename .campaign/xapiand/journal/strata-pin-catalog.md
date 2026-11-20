# strata-pin-catalog: journal

State: waiting on jobs 261120-015212-3d9e92 261120-015214-cc9845 261120-015215-bf532f
Quest: .campaign/xapiand/quests/strata-pin-catalog.md (the lead's; never edit it)

## Doing

Committed eef8d64b2: strata pin 397c02d5 -> e3edb426, reproducer from Xapiand:7b89dc9f3. No API
changes affect Xapiand (strata diff touched catalog.hh, index.hh, section_io.hh's unused
chunked_spill, work_pool_executor.hh; no Xapiand use of section_io). Local ASan build of test_pool
under quiet light running; job scripts in ~/.scratch/xapiand/strata-pin-catalog/.

## Next

1. Wait on the jobs; local ASan test_pool passed once (1647 checks). A2 reverts the pin by sed in its own slot, then builds.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261120-015212-3d9e92 (5 ASan+UBSan runs) |
| A2 | job 261120-015214-cc9845 (--fails-without, old pin) |
| A3 | job 261120-015215-bf532f (campaign verify) |
