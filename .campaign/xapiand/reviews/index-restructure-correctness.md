# index-restructure review: correctness

Commit: strata:cfa20b4191cc
Verdict: APPROVE
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The verdict covers the code. Landing still needs A1 at cfa20b41 (build-test 261124-025827-57b6da
was queued and CI was running when I reviewed), which is the lead's check, not this review's.

## Evidence I relied on

- Step 1 (88058f27..351eebc5): members 261124-020132-e99917 (202 definitions moved, 0 warnings),
  fnsizes 261124-004345-fff00d, selfcontained 261124-015755-58916c, build-test 261124-020132-30df0e
  and CI 45358702484, all at 351eebc5. I did not re-derive the move. I spot-checked two durability
  bodies (collect_deletes, requeue_delete) between the merge base strata:7214f27d and 351eebc5:
  identical after de-indenting.
- Ledger unit test 261124-025155-288e60 and the four mutants 261124-025159-f8ed6f (track-adds),
  -025203-ff830f (forget-drops), -025207-4882f1 (post-keeps-file), -025211-9ae8da
  (unpost-keeps-flight), all at strata:3db2a49e. 3db2a49e..cfa20b41 touches only
  test/reclaim_bg_test.cc, so they stand for cfa20b41. Each log shows the mutant built, then the
  named case failing (for example `FAIL: track: a grown file counts its new size`, `mutant
  track-adds: the test fails (killed)`).

## What I checked myself

1. Every site. `git grep -w` for files_, on_disk_, posted_, inflight_, unlinked_, track,
   forget_file and remove_file at acccd5f2 and at cfa20b41 (include, src, test): every old use in
   basic_index is gone and each maps to one ledger call. Mutations, old to new:
   - `track` (open's scan, write_unsynced, flush_active, flush_documents, append_edit,
     install_merge twice, abandon_merges, reclaim_files twice, requeue_delete) to
     `ledger_.track`: same body (`files_[name]`, `on_disk_ += size - at`).
   - `forget_file` (reclaim_files' ENOENT path, remove_file) to `ledger_.forget`: same body.
     remove_file still removes first, then forgets: bytes go to unlinked only after `io_->remove`.
   - reclaim_posted's inline post to `ledger_.post`: same body, still before
     `del_group_->post`; the throw path still requeues.
   - collect_deletes' inline loop to `ledger_.removed`; requeue_delete's find/erase to
     `ledger_.unpost` (false on a name not posted, the old early return), then `track(left)`.
   - `unlinked_ = 0` in sync_dir and sync_inherited to `ledger_.synced()`, each still after the
     directory sync returns, so no byte is counted freed before its unlink is durable.
   - publish_merges' `unlinked_ += job->removed` to `add_unlinked`.
   - Reads: deletes_pending, close_ready, finish_deletes (copies posted() before requeueing),
     retain and kept_files iterate files(), load_files takes `const std::map*` (index.hh:2830-2831,
     so the old `&files_` was never written through), drain/drain_pending, settle_storage's
     `counted()` equals `on_disk_ + unlinked_ + inflight_`.
   No site changed its position relative to IO, the turn log or the durable() fence.
2. Behavior note 1 (open). In recover (index_open.hh), every name parse_name accepts is tracked
   before either `size_of(manifest_name(chosen))`; `chosen` comes from `logs`, which holds only
   parsed manifest names, and parse_name accepts only canonical numbers (no leading zero,
   index.hh:126), so `manifest_name(chosen)` is exactly a scanned name and the entry always
   exists. Nothing between the scan and these reads removes an entry. With the entry present,
   operator[] and size_of return the same value and insert nothing. If it were ever absent, the old
   code would have left a 0-byte phantom in files_ that retain() would then list for removal; the
   new code leaves none. No later lookup, total or reclaim decision differs.
3. Behavior note 2 (rename). The old `files_.erase(tmp); files_[name] = bytes.size()` left
   `on_disk_` alone; the new `rename` subtracts size_of(tmp) and tracks name at it. write_unsynced
   tracks tmp at exactly bytes.size() on its only success path (index_flush.hh:56), and name is
   never on record (next_generation_ = g + 1 before the attempt, D3 puts g above every manifest and
   tmp the scan saw), so on_disk and files() come out identical. On a replace failure neither old
   nor new touches the ledger and durable() fences. A crash between the rename and the ledger
   update loses only memory; reopen rescans the directory and removes the orphan tmp, unchanged.
4. Invariant. `consistent()` states the figures the old code kept by hand: settle_storage's permit
   is `on_disk + unlinked + in_flight`, which counts each file once only if on_disk and in_flight
   are their maps' sums and no name is in both maps. The sums are checked at the index level by
   test/reclaim_bg_test.cc:155-159 and :457-458, which recompute them from the maps and compare
   with the counters, plus a survey of the real directory (:160, :471). Disjointness and
   `consistent()` itself are checked only in the unit test (see N1).
5. reclaim_bg_test's accessor edits (cfa20b41): five lines, each the same read through the
   ledger. sum_files and sum_posted still iterate the maps and are compared against the separate
   counters, so the assertions are the same and not tautological.
6. Call-site mutants (probe job 261124-030309-f48c48,
   `.scratch/xapiand/review-index-restructure-correctness/callsite_mutants.py`): three wrong
   ledger calls in the index (roll_over's rename replaced by a track that keeps tmp on record;
   collect_deletes' `removed` skipped; sync_dir's `synced` skipped), each run against reclaim,
   reclaim bg, reclaim_bg, reclaim_backlog, media and resource_budget. Result: still queued at
   handback. It asks whether index-level tests already guard these call sites, which is coverage
   that predates this step, not this step's correctness, so the verdict does not wait on it. The
   lead can read it with `campaign job log 261124-030309-f48c48`: a SURVIVES line means that call
   site has no index-level guard (worth a hunt row, not blocking).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | gap | `consistent()` is called only by test/index/ledger_test.cc; nothing in basic_index or its suites checks it, so the "no name both on disk and posted" half of the invariant is unchecked at the index level. The old code had no check either, and the sums are checked by reclaim_bg_test. | `git grep consistent()` at cfa20b41: index_ledger.hh:92 and ledger_test.cc only | Optional: `CHECK(ix.ledger_.consistent())` in reclaim_bg_test's accounting helper (line 155), or a Debug-only assert in settle_storage |
| N2 | no | gap | No mutant covers `rename`, the transition whose shape changed; rename_case would kill one (it checks `!files().count("tmp")` and on_disk). The model test never calls rename or add_unlinked. | test/index/ledger_mutants.py:22-43; ledger_test.cc:80-89, 92-158 | Optional: a `rename-keeps-from` mutant (drop `files_.erase(from)`) named against `rename` |
| N3 | no | nit | `rename` of a name not on record tracks `to` at 0, where the old code set it to bytes.size(). Unreachable (write_unsynced always tracks tmp first), but the transition silently accepts a precondition violation; so do `post` twice (in_flight double-counts) and `track` of a posted name, which the model test guards by construction. | index_ledger.hh:25-30, 43-53; ledger_test.cc:117, 131 | Optional: Debug asserts of the preconditions in the ledger |
| N4 | no | nit | rename drops a replaced `to`'s bytes outright rather than moving them to unlinked until the sync; unreachable here and the next line is sync_dir. | index_ledger.hh:25-30 | none needed |

## Not run

The call-site mutant probe 261124-030309-f48c48 had not run at handback. The full suite and CI at cfa20b41 (build-test 261124-025827-57b6da and CI are the lead's A1). No
Debug or sanitizer build. No re-derivation of step 1's 202 moved definitions beyond two spot
checks. No crash model: step 2 moves no IO and changes no on-disk byte, so I reasoned about the
crash between rename and ledger update from the code rather than running one. Session note: I ran
as a subagent of the lead, not as a titled session of my own.
