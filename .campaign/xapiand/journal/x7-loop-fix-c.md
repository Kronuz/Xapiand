# x7-loop-fix-c: journal

State: waiting on jobs 261112-060150-6f4cc5 261112-060244-26054a 261112-060247-915bc9 261112-060250-fff57b 261112-060253-438fbd 261112-060256-a08cb1 261112-060259-0555e1 261112-060347-eb02a3 261112-060350-41b974 261112-060352-3a68bf
Quest: .campaign/xapiand/quests/x7-loop-fix-c.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Design ready (A1): `docs/perf/x7-loop-stalls.md`, "Design: fix (c), RESTORE's writes off the loop, one shard per
child". Grounded in the A3 job's RESTORE samples (261112-012857-a77bf8, arms 3/6: loop's write in the turn >= 54-55%,
`finalise_write` 46%) and the A2 job's per-loop lag (261112-012855-b34e3c: idle p99 512 us, loaded p99 3.0-5.1 ms).
Sample parser for the shares: `.scratch/shares.py` (untracked).

## Next

0. Read the jobs at Xapiand:a944a38d4 (State lists them). Suite (A4) 261112-060150-6f4cc5. Mutants (A4,
   `--fails-without`, each exits 0 only when its named check's FAIL line appears): sub-run reversed 261112-060244-26054a
   (GET `v` check); writes on the loop 261112-060247-915bc9 (`0.2` check); children one at a time 261112-060250-fff57b
   (peak check); no join before a new numeric id 261112-060253-438fbd (same_items vs serial path); a missed write
   fails 261112-060256-a08cb1 (no `status` item); guard skips restore on a throw 261112-060259-0555e1 (engines_inline
   0). Submitter: `.scratch/mutants.py` (untracked). A mutant that exits 2 or 3 is a build or sed failure: fix the
   sed, resubmit.
1. Timing build 261112-060347-eb02a3 (after the suite), then A3 261112-060350-41b974 (T12, exclusive) and A2
   261112-060352-3a68bf (T4/T5, exclusive), both after the build (`.scratch/timing.py`, x7-loop-stalls' commands with
   the fix's binary). Record both rows in the note's "At the fix" table as fix (c)'s column, and the Evidence table.
2. If every A4 job passes and A3 meets 1.0x: State "ready for review" (A2 is a recorded MISS, see Blocked on).

Done this session:
- The numeric-id failure is pre-existing, not fix (c)'s: diagnostic 261112-054313-bda701 (the serial path forced,
  `parallel = false`) gives the same object 31 = 133 and 39 documents as the parallel path. `least_used_shard` reads
  `written_count`, which counts a document replaced while still buffered twice (strata's `buffer::docs` counts every
  non-tombstone record), so id-less placement in one RESTORE differs from one object per request whenever the buffer
  flushes in between. A simulation of both counting rules reproduces both id lists exactly. The test now compares
  against the same RESTORE with every write on the serial path (`DocIndexer::seams.serial`), plus equal totals
  (Xapiand:a944a38d4); diagnostic prints removed.
- Allowlist rows added ahead of the generic ones (Xapiand:b8ccb017a); `async_check --strict` clean locally.
- E4 recorded in the note (Xapiand:92523fbea).

## Blocked on

- Lead: T4/T5 (A2) cannot pass from this file set. E4 (261112-042106-305a1a): loops 56% busy; turns of 1 ms or more
  (36/s per loop) run at 0.45 CPU per wall second; busy periods (2.1 ms mean) are queues of ~1 ms turns; probe lag
  p99 6-8 ms. Loops are descheduled mid-turn: 16 loops + a WorkPool thread per CPU + the generator on 16 CPUs.
  Proposal, a sibling quest "loop scheduling": fewer runnable threads than CPUs (e.g. HTTP loops or WorkPool threads
  sized so loops + workers <= CPUs) or loop threads at a higher QoS class than workers on macOS
  (`pthread_set_qos_class_self_np`), measured by x7-gate's T4/T5 procedure; fix 3 (frame buffer, `_terms` keys)
  shortens the turns' CPU and is a second sibling. Recommendation: move A2 to those quests; A2 here is run once for
  the record (261112-060352-3a68bf).
- Lead (finding, pre-existing, not blocking): `written_count` (src/engine/strata/shard.cc, via strata's
  `buffered_count`) over-counts documents replaced while buffered, so `least_used_shard` skews: a new numeric id can
  take a shard whose next id a later explicit id overwrites. Outside the file set; a small engine-adapter quest if
  balance or id collisions matter.

## For x7-gate

Amendment text for `docs/design/phase7c.md` (lead routes it; it does not block this quest):

- D4, the chore-class table's `flush` row, its work column gains: "RESTORE writes: a shard's writes in its turn,
  engine chores inline (x7-loop-fix-c)".
- D12, the RESTORE sentence becomes: "A RESTORE prepares its plain objects in query chores of its own group, and
  writes them in flush chores of that group: a range's writes are placed in one turn per shard, and a child task per
  shard, at most W = min(sub-runs, max(1, (threads - 1) / 2)) at once, holds that shard's turn while each chore writes
  a slice of about 1 ms with the engine's chores inline. Its loop places, joins and records results; a new numeric
  id, and a write whose document left since placement with every later write of its id, take the serial path after
  the join, in input order (docs/perf/x7-loop-stalls.md, fix (c))."

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | [artifact] `docs/perf/x7-loop-stalls.md`, "Design: fix (c), RESTORE's writes off the loop, one shard per child" (review pending) |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Each shard's child runs under `engine::counted`, so "a task holds one index's turn at most" holds per child
  rather than being relaxed; the parent holds no turn while it awaits them.
- The write chore reuses `commit_blocking`'s shape (inline engine chores, `BlockingAllowed`, `sync_wait`) as one
  guard for both, rather than splitting strata's write API (outside the set).
- Placement asks each shard once per range; `NEW_NUMERIC_KEY` and a vanished `must_exist` document fall back to the
  serial path after a join, so placement equals a serial restore's exactly.
- Turns are held one chore (about 1 ms) at a time, not per object: fewer hops; the same-index row reports the cost.
- The batch-end drain stays (D1's batch rule unchanged); carrying a run across batches is the next lever only if A3
  falls short.
- New numeric ids are modelled by the serial path in one RESTORE (a test seam), not one object per request, whose ids
  depend on buffer flushes through `written_count` (pre-existing); the design note's test list says so.
- The allowlist gets two specific rows ahead of the generic ones, rather than raising the generic counts, so each
  new hit carries its own reason.
