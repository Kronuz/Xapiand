# hunt-manifest-log-wallclock: journal

State: ready for review
Quest: .campaign/xapiand/quests/hunt-manifest-log-wallclock.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A3 (form chosen): ratio to a same-process calibration. In `deletes` (test/manifest_log_test.cc), the
128-edit apply and the log's open read are each the fastest of five runs divided by the fastest of
five reads of the same snapshot alone. Counting work does not fit: the library has no counter
(test-only quest), the harness's operator-new byte count is off under TSan and cannot see a walk
(clumped and sparse allocate nothing), and the at_bound check beside it already uses ratios.

Healthy ratios (probe 261123-173046-c009d9, Release, strata:761da2c7): apply 0.001 / 0.009 / 0.031
(10M deleted / clumped dense / sparse below), log read 0.99 / 0.97 / 1.14 (snapshot unit 22 / 2.5 /
2.1 ms). Bounds set at strata:277e595d: apply < 2, read < 4 (60x over healthy apply; the 8.9 s
regression is about 400x the unit). time_scale and its sanitizer macro removed.

A2 mutant: 7b246c1f (not on the branch, parent 277e595d) adds `auto rewrite = ordinals()` after the
dense mark in log_deleted::insert, the per-edit ordinal-vector rewrite.

## Next

1. CI (A4) at strata:277e595d: its sanitizer runs are the first check of the bounds under ASan/TSan; on a failure read its ratios before touching a bound.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261123-181130-89410d (build-test at strata:e0a1c78fa516); earlier 261123-174439-9fb117 at 277e595d passed |
| A2 | 261123-181130-89410d (passes at the head), fails without: 261123-174446-9bce6e at mutant strata:7b246c1f (10M deleted apply 116.5x, read 148.8x against bounds 2 and 4) |
| A3 | `test/manifest_log_test.cc`: "snapshot's own read in the same process: the machine's speed cancels." |
| A4 | CI at the landing commit |

Probes examined: 261123-172410-fa7b2b (interrupted after 7 s, resubmitted), 261123-173046-c009d9 (healthy ratios, passed).

## Result

`test/manifest_log_test.cc` `deletes`: the 128 one-ordinal delete edits' apply and the log's open read
are each the fastest of five runs over the fastest of five reads of the same snapshot alone (same
process), bounded apply < 2 and read < 4 for the 10M-deleted, clumped dense and sparse-below sets.
`time_scale` and the sanitizer macro are gone (no user left). Test-only, no library change.

| | apply | read |
|---|---|---|
| healthy (Release probe 261123-173046-c009d9) | 0.001 / 0.009 / 0.031 | 0.99 / 0.97 / 1.14 |
| mutant (261123-174446-9bce6e) | 10M: 116.5, clumped: 99.0 | 10M: 148.8, clumped: 54.3 |
| bound | 2 | 4 |

Healthy sits 60x under the apply bound and 3.5x under the read bound; the mutant is 25x to 58x over.
Open: the sanitizer runs under CI are the first check of the bounds under ASan/TSan; main has moved
(41 files), a merge is the lead's.

## Rulings I made

- Ratio over counting work: no library counter (test-only), operator-new bytes are off under TSan and
  blind to the walks; the at_bound check beside it already uses ratios.
- Read bound 4 matches at_bound's; the read's healthy ratio is about 1 because the edits decode as
  cheaply as the snapshot's own bytes.
