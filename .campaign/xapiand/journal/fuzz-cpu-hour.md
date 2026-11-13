# fuzz-cpu-hour: journal

State: ready for review
Quest: .campaign/xapiand/quests/fuzz-cpu-hour.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done. Prospect branch `quest/xapiand/fuzz-cpu-hour` at `bced9d0` (pushed; CI runs on it): cuts (`4eea54e`),
`tools/release_rows/fuzz_hour.sh`, `docs/perf/fuzz-targets.md` with the cost tables, the four CPU-hours and A3.

## Next

1. Lead: review; check prospect CI on `bced9d0` (the doc is the only change since `43adb89`, the commit the jobs ran at).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | prospect `docs/perf/fuzz-targets.md` (`bced9d0`): per-input cost and causes; libFuzzer cost before/after from jobs 261113-081412-4637f9 and 261113-081415-b2fb3a |
| A2 | 261113-081451-5358c3, 261113-081455-3f4ada, 261113-081459-02662d (segment); 261113-081502-e083e7, 261113-081506-55d102, 261113-081510-b10410 (postings); 261113-081514-9eda0f, 261113-081517-5a8e7e, 261113-081521-9f4bc1 (values); 261113-081524-15b135, 261113-081528-c7f8ca, 261113-081531-084b60 (fielddict); all passed at 43adb8957b95 (docs-only changes since) |
| A3 | 261113-081451-5358c3, 261113-081455-3f4ada, 261113-081459-02662d: `-max_len=256 -len_control=0`; third 3's log reads `lim: 256` throughout (the release-rows runs ended at `lim: 26`) |

## Result

Per-input cost under libFuzzer, before (`61e09a6`) to after (`4adadb9`), ms per input: segment 36.29 to 28.22
(22% cheaper, 27.6 to 35.4 exec/s), postings 20.92 to 17.88 (15%, 47.8 to 55.9), values 4.84 to 4.95 (unchanged),
fielddict 20.51 to 15.49 (24%, 48.8 to 64.6). Causes (segment: exercise walks the damaged base's documents, 96%;
postings: builds a segment per input, 46%; fielddict: rebuilt its dictionary per input, 29%) are in the doc.

One CPU-hour per decoder, 0 findings in all four (3 x 1,200 CPU s, corpus carried across):

| Decoder | CPU s | Executions | Final edges / features | Corpus | Peak RSS |
|---|---|---|---|---|---|
| segment | 3,639 | 137,980 | 6,763 / 19,692 | 1,461 units | 430 MiB |
| postings | 3,642 | 135,867 | 3,928 / 13,704 | 866 | 415 MiB |
| values | 3,646 | 623,388 | 2,503 / 4,636 | 483 | 404 MiB |
| fielddict | 3,669 | 221,349 | 4,840 / 21,717 | 1,435 | 550 MiB |

Open: postings is flat in edges over its last 1,200 CPU s; segment, values and fielddict still gain a few edges
and tens to hundreds of features, so one hour is not a plateau there. Per-third coverage is in the doc only for
the last third (the driver overwrites `TARGET-1.log` each third; the job logs keep executions and new units,
not coverage). Segment's cost-after includes the third-base cut, and `cost` mixes libFuzzer's instrumentation
(its exec/s is the replay's, not a run's).

## Rulings I made

- Thirds, not `--long`: the lead's `--long` is refused as evidence; each hour is three chained 1,200 CPU s jobs.
- Coverage columns are the last third's `DONE`/`INITED` lines (first thirds' logs were overwritten); stated in the doc.
- Executions of a third include its short second round when the first ended under budget (values, fielddict).
