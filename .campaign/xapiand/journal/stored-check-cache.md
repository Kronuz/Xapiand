# stored-check-cache: journal

State: waiting on jobs 261110-182036-44d0ae 261110-182037-74c93c
Quest: .campaign/xapiand/quests/stored-check-cache.md (the lead's; never edit it)

## Doing

Fix and test committed at strata:71750940; build-test and the revert job queued. The branch is
not pushed: contributors never push, so the lead pushes `quest/xapiand/stored-check-cache` for CI.

## Next

1. When both jobs pass: State to ready for review (correctness lens).

## Blocked on

Nothing (the push for CI is the lead's).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261110-182036-44d0ae at strata:71750940 runs `reader_check` (`test/reader_check_test.cc`); revert job 261110-182037-74c93c restores reader.hh from 15d39c30 and must see reader_check_test fail |
| A2 | build-test 261110-182036-44d0ae at strata:71750940 |

Local runs (quiet light, macOS x86_64, RelWithDebInfo): `reader_check_test` without the fix fails
its three cases (12,000, 12,000 and 16,574 allocations for 6,000 documents in 2, 2 and 71 frames);
with it, 2 allocations each, and `format14_test` stays ok.

## Result

`stored_plugin::check` (`include/strata/core/reader.hh:569-572`) reads every record through one
`stored_frame` cache, so each frame decodes once: allocations per check go from one or two per
document to the frame buffer's growth (2 on the test's segments). Its report and signature are
unchanged. Found by postings-fuzz-memory, where prospect's `fuzz_postings` churned up to 189 MB
per input through this check and grew past libFuzzer's 2 GB RSS limit under ASan.

## Rulings I made

- The test calls `stored_plugin::check` directly, so the bound covers the stored check alone
  (2 x frames + 2 allocations), not `check_segment`'s other plugins.
- Under a sanitizer the allocation functions are the runtime's, so there the test checks only the
  report (as `get_alloc_test` does).
