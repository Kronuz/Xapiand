# prospect-pool-reader-transient: journal

State: ready for review
Quest: .campaign/xapiand/quests/prospect-pool-reader-transient.md (the lead's; never edit it)

## Doing

Done; waiting on the A1 job 261124-181031-a6c46d (TSan guard in the test) and the build-test rerun at the round's end.

## Next

1. Read 261124-181031-a6c46d; CI on prospect:c99e6be (linux-tsan failed before at prospect:e6cfcd5 on the test's `operator new` replacement colliding with TSan's runtime; fixed by prospect:c99e6be).
2. correctness review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | test 261124-160815-50fda7 (prospect:c4e3e64), rerun 261124-181031-a6c46d at prospect:c99e6be (only the TSan guard changed: heap checks identical elsewhere); mutant `mutant/prospect-pool-reader-transient/scan-through-pool` 261124-160817-d40a36 (killed on the heap CHECK; not rerun: check_fields' scan is unchanged) |
| A2 | build-test 261124-160820-8db0ed (c4e3e64), reruns at the round's end; CI |
| A3 | strict 261124-160830-97749f (base prospect:44303965ec66, fix prospect:c4e3e64, A/A) |
| A4 | [artifact] CHANGELOG.md (new file; prospect had none) |

## Result

check_fields scans a transient pool reader: 416 B less heap per segment (640 -> 224 B) after a check, the same at 4 and 10 fields (1.7 KB per index at 4 segments, 4.2 KB at 10); the check itself 470-510 ns/segment faster; reader open unchanged. Trade: the first pooled query after a check builds the pool reader again, +1.57 us/segment (4 fields, +40%) and +1.70 us (10, +29%). docs/perf/fielddict-pool-reader.md has the table and the profile: the repeat is `fst::map` validation (~960 ns for 24-54 pooled terms; trusted ctor 0 ns), not the block checksums (verify_in runs per block open). It cannot be removed in fielddict.hh: termdict::reader always validates in its constructor and exposes no trusted constructor. Follow-up for the lead: `termdict::reader(blocks, index, fst::trusted)` plus a bit in fielddict::reader (spare padding) set by check_fields, so pool()'s lazy build skips the second validation after a check; refusals unchanged.

## Rulings I made

- A1's heap probe is a replaced global `operator new`/`delete` counting live blocks in test/fielddict_test.cc: check_fields leaves it unchanged, a pooled lookup raises it once, a later check and lookup leave it. The pooled-field-id refusal is tested on a pool built with termdict::builder holding the key 0xFF"term" under an empty directory, because a flipped byte in a built pool trips the block structure ("terms checksum") first.
- A3's harness is test/fielddict_pool_heap.cc (+ .sh to compile it against a tree's include/ with a build dir's flags, + _ab.sh for base/after/A-A): prospect has no open-only bench, and CMakeLists.txt is outside the quest's file set, so it is not a ctest or CMake target. It measures the fielddict level (heap per segment with malloc_size / malloc_usable_size), not a whole index; the saving is per segment, so per index it is segments x that.
- prospect had no CHANGELOG.md: created.
- The repeat build was not removed (lead's request to try): it needs a termdict API change outside the file set; numbers in docs/perf/fielddict-pool-reader.md. A3 not rerun because the code under test did not change (the A3 job's inputs exclude the test file).
- TSan: the heap probe is compiled out under ThreadSanitizer (its runtime defines operator new); CI's linux-tsan failed at prospect:e6cfcd5 on that.
