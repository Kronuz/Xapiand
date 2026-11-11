# placement-without-flush: journal

State: waiting on jobs 261110-195117-8fdb0e 261110-195119-1ec61f 261110-195120-40e989 261110-195120-5a8344 261110-195146-fe5e97 261110-195147-a43df6 261110-191610-d043da
Quest: .campaign/xapiand/quests/placement-without-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Folding in the correctness review (CHANGES at f3c7d37cb): F1, N1, N2. Jobs in State are at the head a09e16664
(or M3 on it, or master for the A2 before).

## Next

1. Read the jobs; move each row's evidence to it; fix anything red.
2. Lead: N1 (below).

## Blocked on

Nothing for the quest's criteria. N1 is the lead's: `harness/async_allowlist.tsv` is outside my file set; its
`src/database/handler.cc [pool] /lock_shard/` count should go from 25 to 24 (`least_used_shard` folded two loops
into one; async_check reports 24 of 25 hits and still passes).

## Review findings

- F1 (blocking, gap: no test of `written_count`'s `buffered_count()` half): `test_new_numeric_ids_spread_over_the_shards_while_buffered`
  in tests/test_node.cc (20 POSTs, no commit, 5 shards: at most 1 apart per shard), a09e16664. Kills M3
  (refs/scratch/pwf-m3: `written_count` without `buffered_count()`): 261110-195117-8fdb0e pending; passes at the
  head: 261110-195119-1ec61f pending, build-test 261110-195120-40e989 pending.
- N1: outside my file set (Blocked on).
- N2: the two forced-interleaving tests' comments now say the held writers wait on a turn in placement's look or
  in the write (a09e16664).
- N3: older races, none in this quest (the reviewer's ruling); nothing changed.
- Coordinator message (review CHANGES, F1/N1/N2, journal evidence at the head only, A2 at the head): done as above;
  A2 head resubmitted at a09e16664 (261110-195147-a43df6, its build 261110-195146-fe5e97; 460eb8 superseded).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | at a09e16664: 261110-195119-1ec61f (the three placement tests) and build-test 261110-195120-40e989, pending; fails without the fix: see below |
| A2 | after 261110-195147-a43df6 at a09e16664 (pending, exit 0 only when all six RESTOREs are under 10 s); before 261110-191610-d043da at master (pending) |
| A3 | build-test 261110-195120-40e989 and e2e 261110-195120-5a8344 at a09e16664, pending |
| A4 | `docs/mechanisms/placement.md` |

## Fails without the fix

- Segment test at master's code: 261110-191050-6a255b exit 0 (69e797c65 = master + the first two tests: every
  shard's highest segment id 224, 8-15 segment files left, 10 of 38 checks fail; the existing-id test passes there,
  ruling 4). The same tests at the fix (88f65db3f): 261110-191052-e8dc37, 1 segment per shard, 1158 checks ok.
- m1 (published view, no refresh, in `written_key`): 261110-191418-21153b exit 0, the existing-id test fails.
- m2 (no cross-shard look, the explorer's experiment): 261110-191419-e7ea4e exit 0, the existing-id test fails.
- M3 (`written_count` without `buffered_count()`): 261110-195117-8fdb0e pending; the reviewer's probe
  261110-193852-f1d945 showed 20 0 0 0 0 under it, 4 4 4 4 4 with the code.

Earlier runs at e7e93fbfa (same src as the head; tests and docs since): build-test 261110-191640-dd5838 and e2e
261110-191642-50ce83 exit 0; the reviewer's reruns at f3c7d37cb 261110-193059-32cbfe, 261110-193059-bfe26c.
Cancelled, not evidence: 261110-190901-44c703, 261110-191519-a3e84d, 261110-191525-744f49, 261110-191613-460eb8.

## Result

The engine seam gains two written reads in the index's turn, with no refresh: `written_key` (strata's
`basic_index::find_key`, buffers then segments) and `written_count` (segments' live documents with pending deletes
applied, plus `buffered_count()`). Placement (least used shard, the cross-shard look for an existing id in
`replace_document_term`, `index`, `index_uploaded`, `delete_document_term`) uses them; reads keep the published
view. How it works: `docs/mechanisms/placement.md`. Numbers: pending A2.

## Rulings I made

1. `written_count` is the segments' live count (strata `segments()`, pending deletes applied) plus
   `buffered_count()`, not the published view's count plus `buffered_count()`: the view needs a reader slot and
   lags the deletes. It over-counts a document rewritten within one buffer (each record counts); the least-used
   rule only balances, so the approximation is recorded in the note's invariant 4.
2. The written reads take each shard's turn, one at a time: strata's writer-side state changes under the lease in
   flush and merge chores, and the write path's own `find_key` reads it in the turn the same way. A request pin
   wakes a demoted index first, so no read maps a body outside the lease.
3. `index()` and `index_uploaded()` (PUT and a stored PUT) look an id up with the written read too: that lookup only
   picks where the write goes, and it flushed a segment per write even on one shard. GET, PATCH and UPDATE keep
   `get_docid_term` (published view, refresh): they return or rewrite what they read.
4. The existing-id test passes at master too (master refreshes, so it sees buffered documents): it guards the fix
   against a lookup that misses buffered documents (mutant m1: published view, no refresh) or skips the cross-shard
   look (mutant m2, the explorer's experiment). The segment test is the one that fails at master.
5. The off-hash document in that test is written straight to one shard (`Shard::replace_document_term` on a pin),
   as an earlier placement rule could have left it: no current rule places a keyed id off its hash.
