# x7-loop-fix-c: journal

State: blocked on lead: B1's defer-only-the-missing mutant has no observable seam in the built tests (see Review findings and Blocked on); A2/A3 timed and recorded
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

1. Lead rules on B1 (Blocked on, first item). If option 2: a before-write seam in `Shard::apply_slice`, a test that
   erases and rewrites the id at the same docid after the first miss, a defer-only-the-missing mutant, one suite job
   and one `--fails-without` job; then `campaign accept x7-loop-fix-c design --confirm B1=<suite job> B2=<suite job>`.
2. Then State "ready for review" and launch the correctness reviewer (the Result section is still to write).

Done this session:
- Jobs at Xapiand:a944a38d4 all exit 0: suite 261112-060150-6f4cc5, six mutants, timing build 261112-060347-eb02a3,
  A3 261112-060350-41b974, A2 261112-060352-3a68bf. Fix (c)'s column and "At fix (c)" are in
  `docs/perf/x7-loop-stalls.md` "At the fix".
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

- Lead (B1, blocks "ready for review"): the design review asked for a seam test and a mutant that defers only the
  missing object. The seam test exists; that mutant does not, and the built seam cannot observe it (Review findings).
  Option 1: accept the recorded argument, with the reviewer's agreement that the mutant is equivalent here, and confirm
  B1 with the suite job and the a08cb1 mutant. Option 2: add a seam before each write in the apply slice (new code in
  `src/database/shard.cc`, in the file set) so a test erases and rewrites the id at its docid between the first miss
  and the next write of the term, and the missing-only mutant fails it. Recommendation: option 2, since the reviewer's
  scenario (docid reuse between i and j) is the case the deferral exists for and today no test reaches it.
- Lead (A2, recorded miss, unchanged from the E4 item below): at fix (c) T4 is 3.07-6.14 ms (all 16 loops over
  1,024 us) and T5's loaded light p99 is 48.3 and 52.8 ms against bounds of 8.5 and 9.4 ms; T5 is 1.8-2.3x worse than
  the previous fix, and the mix's heavy searches 0.75x of it (1,006 against 1,332-1,364/s). Move A2 to the loop
  scheduling quest; consider whether the apply's worker occupancy (design risk, note line 318) is part of the T5 loss.
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
| A2 | 261112-060352-3a68bf: T4 MISS (3.07-6.14 ms, all 16 loops over 1,024 us), T5 MISS (48.3, 52.8 ms; bounds 8.5, 9.4); recorded miss, Blocked on |
| A3 | 261112-060350-41b974: RESTORE 1.001x pre-7c in `loadtest.py` (pairs 0.981-1.047), 1.019x in `profile_bulk.sh` (0.999-1.094); met at the median, inside the spread |
| A4 | suite 261112-060150-6f4cc5 (Xapiand:a944a38d4, exit 0); fails without: 261112-060244-26054a, 261112-060247-915bc9, 261112-060250-fff57b, 261112-060253-438fbd, 261112-060256-a08cb1, 261112-060259-0555e1 (each exit 0: its named check's FAIL line appeared) |

## Review findings

Design review `.campaign/xapiand/reviews/x7-loop-fix-c-design.md` (CHANGES at Xapiand:45815797d). Fixes: the design
text in Xapiand:9c38ae630, the code and tests in Xapiand:3c3cf24c4; suite 261112-060150-6f4cc5 at Xapiand:a944a38d4.

- B1 (a miss defers every later object of its term): NOT FULLY COVERED. Fix: `Shard::apply_slice`'s `missed_terms`
  (`src/database/shard.cc`), Xapiand:3c3cf24c4. Test: `test_a_write_missed_since_placement_is_placed_afresh`
  (`tests/test_loop_yield.cc`): after placement a seam erases `k7` and writes it again, the stream holds three `k7`
  objects; expects 3 deferred, no failing item, the last object stored. Suite: 261112-060150-6f4cc5 (passes).
  Mutant: 261112-060256-a08cb1, a miss fails as today's `must_exist` miss does (the "no `status` item" check fails);
  it exits 0. Gap: the reviewer's mutant, deferring only the missing object, was not run, and the note's own
  argument says it is not observable with this seam: every later write of `k7` carries the same placement, so it
  misses too and the result is identical. The reviewer's interleaving (the docid reused by a PUT between the first
  miss and the next write of the id) has no seam in the built code, so no test reaches it. Stopped here, no new
  code; options under Blocked on.
- B2 (each test asserts its case happened): COVERED. Fix: `DocIndexer::stats`, `DocIndexer::seams` and `FixedRanges`
  (objects per prepare, not wall time), Xapiand:3c3cf24c4, namespace-scope declarations Xapiand:7e19c4475. Each case
  asserts it happened: model test `largest_range >= 16` and `repeated_in_subrun >= 1`; numeric ids `largest_range >=
  16` and `serial_after_join >= 2`; miss test `erased >= 1` and `deferred == 3`; throw test `failed >= 1`; off-loop
  test `process_ms > 200`; parallel peak asserted when `children > 1` (a pool of 5 threads or more, as designed).
  Suite: 261112-060150-6f4cc5. Mutants (each exits 0, so its case happened): reverse sub-run 261112-060244-26054a,
  no join before a new numeric id 261112-060253-438fbd, children one at a time 261112-060250-fff57b, writes on the
  loop 261112-060247-915bc9, guard skips restore 261112-060259-0555e1.
- N1-N7 are nits and gaps not blocking; N5 is covered by the throw test and mutant 261112-060259-0555e1. The rest
  are the lead's to triage; none was re-examined this session.

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
