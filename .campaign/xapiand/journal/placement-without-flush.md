# placement-without-flush: journal

State: waiting on jobs 261110-191418-21153b 261110-191419-e7ea4e 261110-191610-d043da 261110-191613-460eb8 261110-191640-dd5838 261110-191642-50ce83
Quest: .campaign/xapiand/quests/placement-without-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on the jobs in State (all at or derived from e7e93fbfa unless named).

## Next

1. Read the jobs; fill Evidence; fix anything red (a failing test gets its fix and a job).
2. Lead: set the quest's checks (build-test, e2e: x7-http-repin's commands), so `campaign verify` reuses jobs dd5838 and 50ce83.

## Blocked on

The lead: `campaign verify placement-without-flush` says the quest has no gate checks. I submitted the
build-test and e2e commands of x7-http-repin's checks as jobs (261110-191640-dd5838, 261110-191642-50ce83);
setting the checks is the lead's.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | head 261110-191052-e8dc37 exit 0 (88f65db3f, same code and tests as e7e93fbfa: node suite 1158 checks ok; each shard of both 5-shard RESTOREs: 1 segment, highest id 1); fails at master's code: 261110-191050-6a255b exit 0 (69e797c65 = master + tests: every shard highest id 224, 8-15 segment files left; 10 of 38 checks fail; the existing-id test passes there, ruling 4); build-test 261110-191640-dd5838 pending; mutants m1 261110-191418-21153b, m2 261110-191419-e7ea4e pending (exit 0 when the existing-id test fails) |
| A2 | pending: before 261110-191610-d043da (master), after 261110-191613-460eb8 (e7e93fbfa); builds 261110-191517-0eba87, 261110-191522-0281ed |
| A3 | pending: build-test 261110-191640-dd5838, e2e 261110-191642-50ce83 |
| A4 | `docs/mechanisms/placement.md` |

Cancelled, not evidence: 261110-190901-44c703 (the base job before the rebase), 261110-191519-a3e84d and
261110-191525-744f49 (A2 measures resubmitted with distinct SIDE identities and a fixed parser).

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
