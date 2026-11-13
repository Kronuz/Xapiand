# x7-loop-fix-c: journal

State: waiting on jobs 261112-204313-122916 (restore-test mutant), 261112-201006-81837c (gate check), the mutant reruns and A2/A3 reruns below
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

Lead, msg: ratio job 261112-201017-d47dfe timed out (exit 124); the bound stays master's 0.7. Restore-test mutant submitted at
Xapiand:0f6edb588, `--for A4 --fails-without --minutes 60`: 261112-204313-122916. Steps 1 and 2 below are done as above.

Merged master at Xapiand:0bbf128d3 (pushed; CI runs). The one conflict, tests/test_loop_yield.cc, is resolved with
master's relative form (loop CPU against an on-loop `update` baseline in the same test), keeping this fix's claims
(5 shards, `DocIndexer::stats` children peak, writes in chores): `test_a_restore_prepares_and_writes_its_objects_off_its_loop`,
bound 0.7 for now (master's). Jobs at Xapiand:0bbf128d3:
- gate check `campaign verify`: 261112-201006-81837c.
- ratio measurement, the test five times (light; load was ~30, so read ratios as relative): 261112-201017-d47dfe.
- reruns of the A4 mutants: 261112-201040-503fe0 (reverse sub-run), 261112-201041-4a66d3 (writes on the loop),
  261112-202556-1a371b (children one at a time; first run interrupted), 261112-201042-dc40d0 (no join before a new
  numeric id), 261112-202557-3315c8 (a missed write fails; first run interrupted), 261112-201043-19e10d (guard keeps
  inline chores), 261112-201043-1d9816 (B1), 261112-201044-0da49c (N4 stale break; same job as the old
  084828/090511), 261112-201045-5f5375 (N4 failed slice fails the rest; the old 090524 is the same job).
- A2/A3 reruns (timing build 261112-201054-12c0c3 first): A3 261112-201054-a09f03, A2 261112-201055-0e58b1.

1. When 261112-201017-d47dfe finishes: read the five `loop CPU:` lines. If the worst chores/on-loop ratio leaves room,
   tighten the bound in the test (comment the measured range), commit, push.
2. Submit the restore-test mutant at that head, `--fails-without --for A4`: copy of master's loop-share-flake mutant
   (261112-120322-64f547): insert `if (true) { co_return false; }` after the `fetch` lambda's opening line in
   src/database/handler.cc, build test_loop_yield (exit 2 on a build error), run ctest -V, exit 0 only if
   `grep -q "test_loop_yield.cc:[0-9]*: CHECK(chores_ms < on_loop_ms"`.
3. Rerun any mutant whose commit predates the bound change; record ids; the quest stays held (D67), not ready for review.

Done this session:
- Suite 261112-084823-54c2d4 failed `with_slices == without_slices + 1` (6 and 6) and mutant 080c00 survived: the tests were
  wrong, not the code. The first object of a new index changes its schema and ends its range alone (`pull`'s
  `changed`), so the failing write was a range of one and its slice had no later write. Both failure tests now restore a
  seed first (Xapiand:b4e872ed6); the note's failure section states the behaviour as built.
- First B1 mutant 261112-083119-7312ec survived (suite 261112-083057-772ce1 passed): `must_exist` is checked by key at the
  docid, and only a numeric id's new document takes an erased docid (`args.new_did`; a string id takes `last + 1`). The
  test now uses numeric ids. D68 / N4 option 2 built in the same commit: `apply_slice` takes `at` by reference and
  ends at a write that failed with the endpoint stale; the child's catch fails one write and retries the rest; seam
  `stale_on_term`. Ruling: a busy pin is tested by the thrown-slice seam, which takes the same catch (pin, turn and
  slice failures all land there); no seam makes `pin()` itself busy.
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

- Lead (B1): resolved by the lead's call, option 2 (seam); built in Xapiand:c5fb7f62d, jobs pending.
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
| A2 | [artifact] `docs/perf/x7-loop-stalls.md`, "At the fix", fix (c)'s column and "At fix (c)" (job 261112-060352-3a68bf: T4 and T5 miss, recorded) |
| A3 | 261112-060350-41b974: RESTORE 1.001x pre-7c in `loadtest.py` (pairs 0.981-1.047), 1.019x in `profile_bulk.sh` (0.999-1.094); met at the median, inside the spread |
| A4 | suite 261112-060150-6f4cc5 (Xapiand:a944a38d4, exit 0); fails without: 261112-060244-26054a, 261112-060247-915bc9, 261112-060250-fff57b, 261112-060253-438fbd, 261112-060256-a08cb1, 261112-060259-0555e1 (each exit 0: its named check's FAIL line appeared) |

## Review findings

Design review `.campaign/xapiand/reviews/x7-loop-fix-c-design.md` (CHANGES at Xapiand:45815797d). Fixes: the design
text in Xapiand:9c38ae630, the code and tests in Xapiand:3c3cf24c4; suite 261112-060150-6f4cc5 at Xapiand:a944a38d4.

- B1 (a miss defers every later object of its term): fix `Shard::apply_slice`'s `missed_terms`, Xapiand:3c3cf24c4.
  Tests: `test_a_write_missed_since_placement_is_placed_afresh` (mutant 261112-060256-a08cb1) and
  `test_a_miss_defers_every_later_write_of_its_id` (numeric id 7, seams `writes_per_slice` and `after_slice`).
  Suite 261112-090456-c67417; mutant, a miss defers only the missing write, 261112-084825-447568 (passed).
- N4 (option 2, D68): a slice ends at a stale-marking failure; a failed pin, turn or slice fails one write and the rest
  retry. Tests `test_a_stale_marking_failure_ends_its_slice` (mutant 261112-090511-afb297) and
  `test_a_throwing_apply_slice_fails_one_write_and_restores_the_engines_chores` (mutant 261112-090524-b3bce6).
  Suite 261112-090456-c67417. Results pending.
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
