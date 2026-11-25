# prospect-pool-reader-transient: journal

State: waiting on jobs 261124-160815-50fda7 261124-160817-d40a36 261124-160820-8db0ed 261124-160830-97749f
Quest: .campaign/xapiand/quests/prospect-pool-reader-transient.md (the lead's; never edit it)

## Doing

Code committed in prospect (branch quest/xapiand/prospect-pool-reader-transient): the field-check-at-map
patch applied (`transient` factored out of `dictionary`; `check_fields` scans a transient pool reader).
Evidence jobs queued at prospect:c4e3e64. docs/perf/fielddict-pool-reader.md waits on the A3 job's numbers.

## Next

1. Read the A1 test, mutant (must fail on the `CHECK(live_blocks == before)` lines, not a build error), build-test and A3 jobs.
2. Write docs/perf/fielddict-pool-reader.md from the A3 job's table; commit with CHANGELOG.md (docs only: the jobs' --inputs exclude docs).
3. Push, State: ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | test job 261124-160815-50fda7 (prospect:c4e3e64); mutant branch `mutant/prospect-pool-reader-transient/scan-through-pool` (prospect:ffe448f) job 261124-160817-d40a36 --fails-without |
| A2 | 261124-160820-8db0ed (build-test); CI on the push |
| A3 | strict A/B 261124-160830-97749f (base prospect:44303965ec66, fix prospect:c4e3e64, A/A of base) |
| A4 | [artifact] CHANGELOG.md (new file; prospect had none) |

## Result

(after the jobs)

## Rulings I made

- A1's heap probe is a replaced global `operator new`/`delete` counting live blocks in test/fielddict_test.cc: check_fields leaves it unchanged, a pooled lookup raises it once, a later check and lookup leave it. The pooled-field-id refusal is tested on a pool built with termdict::builder holding the key 0xFF"term" under an empty directory, because a flipped byte in a built pool trips the block structure ("terms checksum") first.
- A3's harness is test/fielddict_pool_heap.cc (+ .sh to compile it against a tree's include/ with a build dir's flags, + _ab.sh for base/after/A-A): prospect has no open-only bench, and CMakeLists.txt is outside the quest's file set, so it is not a ctest or CMake target. It measures the fielddict level (heap per segment with malloc_size / malloc_usable_size), not a whole index; the saving is per segment, so per index it is segments x that.
- prospect had no CHANGELOG.md: created.
