# field-check-at-map: strata checks a segment's field ids wherever it maps

Risk: high (decoder safety on corrupt segments). The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status field-check-at-map`.

## Outcome

A segment whose field ids fall outside its manifest's field table, or name the key field, is
refused with `corrupt_segment` wherever it maps: an eager open, a lazy segment's first read, a
merge's inputs, a wake. Today only Xapiand's open-time loop (`verify_fields`) checks it, so a
corrupt index opened by any other path (and every merge input) is read. The manifest's claim a
mapped file must meet (`vouch`, index.hh) carries the field-table size and key field from the
manifest, and the map calls each plugin reader's `check_fields` when it has one. A segment this
process wrote keeps the default vouch, which bounds no field. In-memory only: no byte changes.

Evidence: the eager-open explorer, 2026-11-21: `docs/perf/eager-open.md` on strata
`explore/eager-open` (strata:7107f9bc), prototype there and on prospect `explore/xapiand/eager-open`
(prospect:3f88c3c); identity job 261121-110632-e10bb1.

## Acceptance criteria

- A1 A manifest resealed with a field dropped is refused at open (eager), at first read (lazy) and
  at a merge's input map, each with `corrupt_segment` naming the field: a test with values (built
  on prospect's inverted plugin, or a test plugin with `check_fields`), with a mutant that skips the
  check at map, `--fails-without`
- A2 Good indexes give the same results and digests with and without the change (the explorer's
  identity fixtures d1000s1, s4, s10): a job
- A3 Open time and memory per open index stay within the A/A band of
  `bench/eager_open` (the explorer's harness, landed with this quest), a `strict` job before and
  after with an A/A
- A4 The suite passes, and ASan+UBSan on the new test: the `build-test` check and CI
- A5 [artifact] `docs/mechanisms/field-check.md` says where the check runs and why
- A6 [ci] CI is green on every platform

## Contracts

Provides strata:segment-check (a plugin reader's optional `check_fields(size, key)`),
consumed by prospect-check-fields and drop-open-verify.

## Decisions

- Lead, 2026-11-21: lazy opens for Xapiand are dropped as a memory or latency lever: on the request
  path the maps move into the same request's first read (open plus first query within noise), and
  an idle index already demotes to warm (11.9 KB, 1 descriptor) after 30 s; lazy saves 0.9 KB more,
  and only for opens no read follows. For many indexes per host the lever is the hot set.
- Lead, 2026-11-21: it starts after turn-log-parity's `split` task has split index.hh, so it does
  not conflict with the split; it may start earlier only with the split's owner told.
- The two-seek pool check (the explorer's experiment 4) joins this quest only if its rows show a
  gain outside the A/A band.

## May land early

Nothing: one landing.
