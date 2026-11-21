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
- A6 At par or better than RocksDB on every row and axis of document-gate's gate report (owner,
  2026-11-20, replacing D143's 1.5x): throughput, p99 to p99.99, time to durable, group commit at
  K writers on 1 to 8 logs, peak memory, disk, bytes written, CPU per operation, on the host and in
  the Linux and FreeBSD VMs; one exclusive row per set after a seconds-long smoke, main merged
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

Parity tasks (D148; strata `docs/design/rocksdb-parity.md`, "Tasks for turn-log-parity", rows 1-12,
each with its files and rows): cohort wait (1), zero-copy turn (2), segment ladder and spare pool
(3), checkpoint inside the window (4), packed turn (5), background flush (6), sorted runs in the merge
policy (7), reopen at a changed loop count (8), refuse a log past the loop count (9), one log per
device after its design review (10), unversioned upsert (11), VM profile (12). Tasks 3, 5, 9 and 10
change journal: Detent's lead gets each diff before it lands and checks it against its authority
suite. Detent's constraints (2026-11-20): journal's I/O surface (`IO`, `IOOperation`, `PosixIO`,
`NativeCompletionQueue`, `AsioCompletionDriver`, `LinuxCompletionPolicy`, `File`, `DirectoryCursor`,
`drive_synchronously`, `MutationRequest`/`MutationCompletion`, `AdmissionLimits`, `PrimitiveKind`) and
its completion ordering stay; a turn is durable before an artifact citing it publishes; a
single-voter, one-loop authority opens its log after task 9. Log format changes need no migration.

Copy flag (Detent ruling R3, journal-double-damage at journal:cca2427): damage that no manifest names
loses every group, loudly. A copy flag in the record format lets recovery keep the groups that write
again after the damage; tasks 3 and 5 (record format) add it, with a test that a group rewritten after
double damage recovers and one that is not still reports the loss.

Journal follow-ups (Detent's journal-double-damage review, journal:178030b): task 9 refuses at
construction a block_bytes that is neither a multiple nor a divisor of 512 (F3); tasks 3 or 5 add a
per-fragment checksum so a damaged fragment crossing a sector, and the damaged headers still silent
at some block sizes, are reported (a format change), beside the copy flag above.

TurnLog::forward_slice scans every live group each turn (Detent, msg-176), so setup grows with the square
of the group count; parked groups leave that scan (journal:c3fb805: 0.01 records visited a turn over 20,000 parked, 193 for a mutant that keeps them); groups held or evicted still cost a walk, so a journal task indexes live records by segment, and and the D5/D7 tasks measure the
turn cost at 1k, 100k and 1M groups with most parked.

Parked groups are one mechanism (lead, 2026-11-20): the durable `parked` state the design review's
F3 asks for is Detent's `journal-idle-groups` (provides `journal:idle-park`); this quest consumes it
and defines no second park. Names (Detent, journal:7b64376): `parked` is the durable table state,
`evicted` the in-memory release. Contract: create and reopen are durable at the group's first turn, not
when the call returns; Xapiand's idle policy calls `Host::park` (Detent docs/design/idle-groups.md). Follow-up tasks from the revised note (strata:e5721fd6): 13 removes forward
copies once parking lands; 14 a per-segment roster narrowing damage scope (16 B a live group); N3, the
cohort wait counting writers rather than opened records.

Needs `document-gate` and `group-core` (both hold `include/strata/core/**` or `index.hh`).

## Contracts

No new public API. Log and checkpoint bytes change (pre-release, D30); `format-freeze` reviews them.

## Decisions

- D103 (owner, 2026-11-17): the gate takes options 1 for Q3-Q5; the gaps close in this feature.
- D104 (per-core log or one shared log) is decided on the measurements below.

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

## May land early

None: it changes log bytes and a durability mechanism.
