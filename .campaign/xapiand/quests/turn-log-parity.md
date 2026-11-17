# turn-log-parity: A logged index matches RocksDB: read-your-writes, tolerant open, large transactions

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status turn-log-parity`.

## Outcome

document-gate takes the turn-log design's options 1 for Q3-Q5 (D103) to reach its gate. This quest
closes the three gaps those choices leave against RocksDB and the other engines, at par or better
(owner, 2026-11-17), as soon as document-gate lands:

1. Q3: a client reads its own `ack=none` writes without exposing crash-removable writes to other
   readers (the design's option 3, or better if a design finds it), at no cost to the gate rows.
2. Q4: one damaged log block no longer fails indexes whose checkpoint already covers their last
   record; the open reports a possible loss only where one exists (option 2, with the catalog).
3. Group commit: concurrent writers on one log reach RocksDB's ops/s and p50 (a writer arriving
   during a barrier no longer waits a whole extra barrier), on one log and on one log per core.
4. Q5: a logged index admits what an unlogged one admits; a transaction over the replicated
   path's 64 MiB command limit is logged by a direct checkpoint or a chunked command, with its
   stall or cost measured.

## Acceptance criteria

- A1 Read-your-writes under `ack=none`: a test with values where the writer reads its write before
  the barrier and another reader does not; a mutant without the mechanism fails it
  (`--fails-without`); the `ack=none` gate rows no worse than document-gate's (strict jobs at both
  commits)
- A2 Tolerant open: a crash-and-damage test where an index covered by its checkpoint opens and one
  not covered fails with `log_lost` or reports its possible loss; a mutant that opens every index
  fails it
- A3 Large transactions: a logged index accepts a transaction above 64 MiB up to the unlogged
  limit, durable across a crash at every point (the crash model); its latency and the stall it
  causes to other indexes on the loop stated (strict jobs)
- A4 A RocksDB comparison row for each gap (read-your-writes latency, open after damage, a 256 MiB
  transaction) in `docs/perf/`, at par or better, or the honest gap with its cause
- A6 Group commit at par: with K = 2, 4, 8 concurrent writers on one index's sync rows, ops/s and
  p50/p99 at par with RocksDB or better (document-gate measured K=4 at 99 against 121-144 ops/s,
  p50 40 ms against 21 ms, job 261117-034940-b2b9c3; a writer arriving during a barrier waits a
  whole next barrier); and the same with one log per core against one shared log, so D104's
  per-core choice is confirmed or revised on numbers (exclusive jobs at both commits)
- A7 Atomic large batches: a kv batch past the 1 MiB command limit is all-or-nothing across a crash, like RocksDB's WriteBatch (document-gate durability F2: 20 of 20 split batches took two or more barriers); a power-loss sweep over the split proves it, with a mutant
- A5 [artifact] `docs/design/standalone-turn-log.md` records Q3-Q5 as closed, with the mechanisms

## Decisions

- D103 (owner, 2026-11-17): the gate takes options 1 for Q3-Q5; the gaps close in this quest, soon.

## May land early

Each gap is separable: A1, A2 and A3 may land as increments, each with its own evidence and review.
