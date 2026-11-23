# turn-log-parity: A logged index matches RocksDB: read-your-writes, tolerant open, large transactions

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status turn-log-parity`.

Whole feature (replan-2026-11-20, section 1 F6; D103: "soon" after `document-gate`).

## Outcome

The four gaps `document-gate` left against RocksDB close, at par or better (owner, 2026-11-17): a
client reads its own `ack=none` writes without exposing crash-removable writes to other readers
(Q3); one damaged log block no longer fails indexes whose checkpoint covers their last record (Q4);
a writer arriving during a barrier joins the next one instead of waiting a whole extra barrier
(group commit); a logged index admits transactions as large as an unlogged one, and a batch past the
1 MiB command limit is atomic across a crash (Q5, `document-gate` durability F2).

## Acceptance criteria

Gate: the suite (`build-test`). The crash model, which gates because this changes a durability
mechanism ("Direct path first"), is A3's job.

- A1 Read-your-writes under `ack=none`: the writer reads its write before the barrier, another
  reader does not; a mutant without the mechanism fails (a test with values, `--fails-without`)
- A2 Tolerant open: after a crash with damage, an index its checkpoint covers opens and one not
  covered fails `log_lost` or reports its possible loss; a mutant that opens every index fails
  (`--fails-without`)
- A3 A transaction above 64 MiB up to the unlogged limit, and a kv batch past the 1 MiB command
  limit, are all-or-nothing across a crash at every point (a power-loss sweep over the split), each
  with a mutant (the crash model, `--fails-without`)
- A4 Group commit: with K concurrent writers, no writer that arrives during a barrier waits more
  than the next barrier, counted per writer; a mutant that defers it a whole barrier fails (a test,
  `--fails-without`)
- A5 [artifact] `docs/design/standalone-turn-log.md` records Q3 to Q5 closed, with the mechanisms

## Tasks

One agent each, merged into the quest branch on its own tests. Strong, high (durability). All four
touch `include/strata/core/**` or `index.hh`; the lead narrows each task's globs to the files its gap
touches at `campaign start`, and tasks whose globs are not disjoint run in order.

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `ryw` | the `ack=none` read path in `include/strata/core/**`, `test/*ryw*`, `test/fixtures/**` | implementation, strong, high | Q3: read-your-writes without exposing crash-removable writes (A1) |
| 1 | `tolerant-open` | the log open and catalog path, `test/*open*`, `fuzz/**` | implementation, strong, high | Q4: tolerant open, `log_lost` (A2) |
| 2 | `large-tx` | the transaction and checkpoint path, `test/*tx*`, `test/*crash*` | implementation, strong, high | Q5: transactions up to the unlogged limit; atomic batches past 1 MiB (A3), with the crash sweep |
| 2 | `group-commit` | the barrier scheduler, `test/*barrier*` | implementation, strong, high | Joining the next barrier, per-writer count (A4); one log and one log per core |
| 3 | `design-note` | `docs/design/standalone-turn-log.md`, `docs/mechanisms/**` | documentation, economical, medium | Records Q3 to Q5 closed with the mechanisms (A5) |

Parity tasks (D148) and RocksDB parity (the old A6) moved to quest `rocksdb-parity` (owner,
2026-11-23); the parity tasks merged here (cohort-wait, checkpoint-window, merge-runs,
reopen-loops) stay as landed parts of this quest.

Needs `document-gate` and `group-core` (both hold `include/strata/core/**` or `index.hh`).

## Contracts

No new public API. Log and checkpoint bytes change (pre-release, D30); `format-freeze` reviews them.

## Decisions

- D103 (owner, 2026-11-17): the gate takes options 1 for Q3-Q5; the gaps close in this feature.
- D104 (per-core log or one shared log) is decided on the measurements below.
- Owner, 2026-11-23: split. A1-A5 land here; A6 and the parity tasks not merged here move to
  `rocksdb-parity`, which A6 gates unchanged. Review N1 (journal lost-bound, first open after
  rule-5 damage plus a power cut) lands as a recorded limit (journal `docs/mechanisms/lost-bound.md`,
  Limits) and is fixed by quest `journal-bound-first-open`.

## When done

Runs once, when the feature is complete and before it lands; findings are fixed on the quest branch,
each with a test that fails without it.

- Review: one reviewer, lens `durability`.
- Measurements, one `exclusive` job of at most 10 minutes each on the macOS host, at this commit and
  at `document-gate`'s: the `ack=none` rows (no worse than `document-gate`'s); the large
  transaction's latency and its stall to other indexes on the loop; a RocksDB comparison row for each
  gap in `docs/perf/` (read-your-writes latency, open after damage, a 256 MiB transaction), at par or
  better or the honest gap with its cause; and group commit at K = 2, 4 and 8 on one log and on one
  log per core, deciding D104 on numbers (`document-gate` measured K=4 at 99 against 121-144 ops/s,
  p50 40 ms against 21 ms, job 261117-034940-b2b9c3).

- Lead, 2026-11-21 (owner's size rule): `include/strata/core/index.hh` (6,139 lines) and
  `manifest_log.hh` (1,303) are split where it helps maintenance (owner, later the same day: "Let's
  not split for the sake of splitting"; a file that is one coherent unit stays whole) by a task `split`, which the lead starts
  right after tasks group-commit and large-tx merge into this branch and before tolerant-open edits
  them; moves only, proved by cpp-libraries' `tools/split_check.py` (quest split-check), with
  `index.hh` kept as the include its callers name.
- Lead, 2026-11-21: the `split` task is superseded by quest `index-restructure` (after this quest
  lands): 5,309 of index.hh's lines are basic_index's class body, which a moves-only split can't cut.

## May land early

None: it changes log bytes and a durability mechanism.
