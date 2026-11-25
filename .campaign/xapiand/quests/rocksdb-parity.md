# rocksdb-parity: strata matches or beats RocksDB on every gate row

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status rocksdb-parity`.

Split from `turn-log-parity` (owner, 2026-11-23): its A6 and the parity tasks not merged there.

## Outcome

strata is at par or better than RocksDB on every row and axis of document-gate's gate report,
through the remaining tasks of strata `docs/design/rocksdb-parity.md`, "Tasks for turn-log-parity"
(tasks 2-6 and 9-14, and the parts of 4 and 8 not merged in turn-log-parity). Its first commit adds
to the design's A2 section the lost bound's first-open limit (journal `docs/mechanisms/lost-bound.md`,
Limits), closed by `detent:journal-bound-first-open`.

## Acceptance criteria

Gate: the suite (`build-test`, asserts on). Rows are measured on main merged into this branch, one
`exclusive` job per row set after a seconds-long smoke; a job is tagged `--for A1` only when it
covers every gate row.

- A1 At par or better than RocksDB on every row and axis of document-gate's gate report (owner,
  2026-11-20, replacing D143's 1.5x): throughput, p99 to p99.99, time to durable, group commit at
  K writers on 1 to 8 logs, peak memory, disk, bytes written, CPU per operation, on the host and in
  the Linux and FreeBSD VMs; one exclusive row per set after a seconds-long smoke, main merged
- A2 Each task that changes a log or record format has a crash test that fails without it (a
  `--fails-without` job), and each journal diff passed Detent's authority suite before landing
- A3 [artifact] `docs/design/rocksdb-parity.md` has the gate table with each row's result at the
  landed commit, and the honest gap with its cause wherever a row is still short

## Tasks

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

## Contracts

Log and checkpoint bytes change (pre-release, D30); `format-freeze` reviews them. Journal changes go
through Detent's lead (tasks 2, 3, 6, 9, 10, 13 and 14).

## Decisions

- Owner, 2026-11-23: A1 is turn-log-parity's A6, unchanged (owner, 2026-11-20, replacing D143).
- Lead, 2026-11-25 (msg-1 search row, light probe at strata:3292724f): sorted runs pay off only where
  reads find one run by key (the range index); a postings query fans out over every run (TOP_10 term
  p99 2.5x slower at 62 runs, about 6.6 us per extra run). So the run cap follows the layout's read
  shape, one mechanism with one parameter: the layout declares it (key lookup: 64; postings fan-out:
  0, tiers only), with the exclusive A/B (64, 16, 0) recorded in docs/perf/search-runs.md. A
  run-aware query path that removes the fan-out cost is a follow-up, not this step.

## May land early

Each task whose rows do not regress may land as an increment with its test; A1 gates the last.
