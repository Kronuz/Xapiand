# index-restructure: journal

State: ready for review at strata:cfa20b41 (pushed); waiting on build-test 261124-025827-57b6da;
members 261124-025756-a4f2ac and 261124-025827-8a1219 at the head are expected red (step 2 changes
code; step 1's members proof is e99917 at 351eebc5)
Quest: .campaign/xapiand/quests/index-restructure.md (the lead's; never edit it)

Code: strata worktree /Users/kronuz/code/Kronuz/strata-wt/xapiand/index-restructure, branch
quest/xapiand/index-restructure.

## Doing

Nothing: ready for review.

## Next

1. Review findings, by id.

## Lead's rulings (2026-11-24)

Answered: file set granted (test/index/**, CMakeLists.txt); build step 2 here after merging main
(merged at strata:acccd5f2, clean: main touched only file_set.hh and its test); keep step 1's
commits (kept: 88058f27..351eebc5 unchanged). Done as below.

## Step 2: the file ledger (strata:3db2a49e)

`index_detail::file_ledger` in include/strata/core/index_ledger.hh (non-template, std only,
included by index.hh before the class), member `ledger_`, replaces files_, on_disk_, posted_,
inflight_, unlinked_ and basic_index::track / forget_file. 11 sites in storage, commit, merge,
open, checkpoint; index_storage.hh -49 lines. Transitions: track, rename, forget, post, removed,
unpost, add_unlinked, synced; reads: files, posted, size_of, on_disk, in_flight, unlinked,
counted; consistent() states the invariant. Tests: test/index/ledger_test.cc (cases track, forget,
post, unpost, rename, model: random sequences against a per-file model), test/index/ledger.cmake,
CMakeLists globs test/index/*.cmake. Mutants (test/index/ledger_mutants.py): track-adds (track),
forget-drops (forget), post-keeps-file (post), unpost-keeps-flight (unpost); each job builds the
target, runs the named case clean (exit 126 otherwise), then the mutant. Docs: index-layout.md
"Subobjects" and "Candidates not extracted" (each remaining candidate with why it stays),
source-layout.md row.

## Merge of main (2026-11-24, strata:351eebc5)

main (strata:7214f27d) changed index.hh in 16 commits, among them strata-journal-pin's bounded
group status in attach_log (lost only when `cur_.applied.index < bound`). The resolution took
main's index.hh whole and reran the move with cpp-libraries' tools/outline.py (bc16496) instead of
the .scratch mover (graduated, per the lead's 11-21 decision): the pieces differ from step 1's only
by main's edits (diff against a7448b3f: turns +116/-27, commit +11, apply -5, flush +1,
checkpoint +1). attach_log's `bounded && cur_.applied.index < opts_.log->bound(g)` is at
index_turns.hh:40. New members mapped: replay_one, log_due, count_writing, join_chunks,
await_sealed to index_turns.hh; commit_empty to index_commit.hh. outline.py matches a bare key
against nested classes' members too (snapshot::get, reading::scan, view::get, reader_handle::live,
merge_spill::drain), so those keys are pinned by line (`get@2066`) and nested members stay in the
class as in step 1. Local dry run of `split_check.py --members --base origin/main`: 202 member
definitions moved, 0 errors, 0 warnings. The map is in the merge commit's message (and
~/.scratch/xapiand/index-restructure/index.map). .scratch/index-restructure/* left the branch
(git rm --cached). class body: 5,413 lines on main, 2,100 at 351eebc5; index.hh 6,267 -> 2,967.

## The map (step 1)

Class body on main: strata:cc30217f index.hh lines 823-6131. 196 multi-line member definitions
(3,468 lines) moved; 52 one-line definitions stay in the class. After the move index.hh is 2,915
lines, the class body 2,071 (lines 823-2894): ~1,100 of nested types, the rest declarations with
their comments and the data members.

| Mechanism | Header | Members (main's line ranges) | Lines moved |
|---|---|---|---|
| lifecycle, recovery, heal, warm | index_open.hh | create..try_finish_close 839-982; heal, needs_heal 1268, 1365-1411; demote, close_ready, promote 1421-1467; warm_quiescent 4999; recovery 5731-6029 | 464 |
| transactions | index_apply.hh | apply 994-1110, abandon, check_in 1170, fence 4043, guarded/live 3142-3170, run_chores 3251, rollback..evaluate 4639-4956 | 471 |
| the write buffer | index_buffer.hh | buffered_count 1842, memory 2668-2760, for_each_buffer 3135, lookups 4521-4618, the buffer 4992-5076 | 233 |
| freeze and flush | index_flush.hh | refresh 1202, streamed_segment/map_output/write_unsynced 3176-3215, identity 4966-4986, freeze..flush_documents 5082-5321 | 285 |
| the turn log | index_turns.hh | await_durable 1123, attach_log..await_log_through 3350-3585 | 224 |
| commit | index_commit.hh | commit 1282, merge_output_unsynced 1192, prepare_commit..live_length 5330-5649 | 299 |
| checkpoints, retention | index_checkpoint.hh | checkpoint, release, install 1930-2034; retain, kept_files, names_of 5659-5726 | 153 |
| storage, reclamation | index_storage.hh | reclaim..wait_deletes 1306-1331, deletes and reclaim 4221-4386, accounting 4405-4517 | 256 |
| background merges | index_merge.hh | merge..wait_merges 1214-1262, run_merges, start_merges 2149-2185, merging..abandon_merges 3683-4195 | 362 |
| reads | index_read.hh | segments 1521, digest, check 1858-1927, find_key..scan 2037-2141, digest_of 2189, scans/runs 2260-2376, reads at a position 2382-2481, segment reads 2496-2663 | 560 |
| shared readers | index_publish.hh | reader, publish_through, views_held 1781-1829; publication 3280-3336, 3593-3679 | 161 |

Written up in strata docs/mechanisms/index-layout.md (what lives where and why) and
docs/source-layout.md (the headers).

## Step 2 candidates (not built)

State and invariants that stand apart, each judged on maintenance when step 2 runs:

1. Storage ledger: on_disk_, reserved_, unlinked_, inflight_, posted_, obsoleted_, debt_,
   truncated_, held_, allowance_, files_, reclaim_, queued_, del_chan_/del_group_; invariant: a
   file's bytes counted once (on disk, posted, unlinked, freed).
2. Turn log state: logged_, visible_, join_owed_, attributes_owed_, replaying_; V never passes
   the synced sequence, the join waits for its attribute's edit.
3. Manifest log writer: log_, appender_, log_g_, log_end_, log_edits_, rollover_owed_,
   next_generation_, deleted_since_, meta_since_.
4. Retention window: kept_files_, pending_pin_, pending_unpin_, retain_owed_,
   retention_blocked_, inherited_owed_, inherited_below_.
5. Undo log: undo_, mark_, rollback's order.
6. defer_io owed turns: drain_asked_, drain_ran_, promote_owed_, map_asked_ (CT-R22).
7. The merge job's nested classes (merge_file, merge_output, merge_spill, merge_scratch) can
   leave the class body for index_merge.hh.

## Blocked on

Nothing. test/reclaim_bg_test.cc granted (lead, 2026-11-24); its five accessors read ledger_ at
strata:cfa20b41.

## Step 2 design (ready to build once 1 is ruled)

First subobject: the file ledger, candidate 1's core. State: files_, on_disk_, posted_,
inflight_, unlinked_ (5 members, arithmetic in 11 places across storage, commit, merge, open).
One member class `file_ledger` in `index_ledger.hh` with named transitions:
track(name, size) (on disk at size), rename(from, to) (commit's tmp -> manifest, bytes stay
counted), forget(name) (removed: on disk -> unlinked), post(name) -> size (on disk -> in flight),
removed(name) (in flight -> unlinked), unpost(name) -> size? (requeue: leaves in flight),
add_unlinked(n) (a failed merge's untracked removed bytes), synced() (unlinked freed); reads
files(), on_disk(), inflight(), unlinked(), posted_count(). Invariant, stated once: on_disk ==
sum of files' sizes, inflight == sum of posted sizes, no name both on disk and posted, so each
file's bytes count once. Tests drive each transition and check the invariant; mutants: track
adds size instead of the delta, forget drops the bytes instead of moving them to unlinked, post
keeps the name on disk, unpost keeps the bytes in flight, each killed by a named case. Two
behavior notes for review: open's `files_[manifest]` (operator[] inserts) becomes a lookup that
does not insert (the scan tracked every manifest, so the entry always exists); commit's rename
was `erase(tmp); files_[name] = size`, the same bytes (write_unsynced tracked tmp at that size,
and the new generation's name is above every name on disk, D3). The rest of candidate 1
(reserved_, held_, allowance_, debt_, truncated_, obsoleted_, the reclaim queue) stays: they are
permit and reclaim-pacing figures, not a file's state. Candidates 2-7 judged after this one.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | step 1 at strata:351eebc5: build-test 261124-020132-30df0e, CI 45358702484; step 2 at strata:cfa20b41: build-test 261124-025827-57b6da (queued), CI on the push of cfa20b41 |
| A2 | D178 (step 1's members, fnsizes and selfcontained proofs at strata:351eebc5 are listed there) |
| A3 | ledger test 261124-034954-48d9cb (passed, every case and the model at 1.2M steps); mutants killed, each by a full build then its named case failing: 261124-025159-f8ed6f (track-adds, `track`), 261124-025203-ff830f (forget-drops, `forget`), 261124-025207-4882f1 (post-keeps-file, `post`), 261124-025211-9ae8da (unpost-keeps-flight, `unpost`) |
| A4 | strata docs/source-layout.md, docs/mechanisms/index-layout.md (strata:3db2a49e) |

## Result

Step 1 (strata:88058f27..351eebc5): basic_index's member definitions moved out of the class body
into 11 mechanism headers with cpp-libraries' outline.py; body 5,413 -> 2,100 lines, index.hh
6,267 -> 2,967; no function changed size (fnsizes, Release+LTO).

Step 2 (strata:3db2a49e, cfa20b41): `index_detail::file_ledger` (index_ledger.hh) replaces five
members and 11 hand-kept update sites with named transitions and one stated invariant; tests and
four killed mutants in test/index/. Docs: docs/mechanisms/index-layout.md (Subobjects, and why
each other candidate stays), docs/source-layout.md.

For the reviewer, two sites change shape, not result:
1. Open's two `files_[manifest_name(chosen)]` reads (index_open.hh, rollover_owed_ and `newest`)
   were operator[], which inserts a 0-byte entry for a missing name; `ledger_.size_of()` returns 0
   without inserting. The directory scan tracks every file before these reads, so the entry always
   exists.
2. Commit's roll-over rename was `files_.erase(tmp); files_[name] = bytes.size()`; it is
   `ledger_.rename(tmp, name)`, which moves tmp's tracked bytes to `name` (and would drop bytes a
   previous `name` held). write_unsynced tracked tmp at exactly bytes.size(), and the new
   generation's name is above every name on disk (D3), so the figures are the same.

## Rulings I made

- R1 Headers beside index.hh as `index_<mechanism>.hh`: strata's include tree is flat.
- R2 One-line definitions stay in the class beside their declarations (52 accessors and
  forwarders); every multi-line definition moves. Moving one-liners doubles them and scatters
  the interface.
- R3 No `inline` on the out-of-class template definitions, against the prompt's literal form:
  templates need none, and clang adds `inlinehint` only to functions declared `inline` (an
  in-class definition is implicitly inline, without the hint), so `inline` could change clang's
  inlining. The fnsizes job checks the result; the mover's `--inline` flag emits it if the lead
  wants it anyway.
- R4 A return type that names a class member (21 members: segment_list, copy, buffer&,
  basic_index, std::unique_ptr<open_segment>, ...) becomes a trailing return type with its text
  unchanged (`auto basic_index<Layout>::f(...) const -> copy`), not a `typename
  basic_index<Layout>::` qualification. split-check-members' mode must accept this shape; for
  the lead to forward.
- R5 Leading doc comments and the section dividers stay in the class with the declarations; the
  definitions carry none.
- R6 Each piece includes index.hh, which includes every piece after the class (`#pragma once`
  breaks the cycle), so a piece compiles alone and callers still name only index.hh.
- R7 The namespace-scope types (~830 lines: io_spill, file_sink, lent_frame, ...) stay in
  index.hh; the quest's outcome is the class body.
- R8 The mover and the fnsizes job script are committed under .scratch/index-restructure/
  (git add -f) for the rerun after turn-log-parity; drop them before landing. Done at the main
  merge: outline.py replaces the mover, the scripts live in ~/.scratch/xapiand/index-restructure/.
- R9 Nested-class members stay in the class (as in step 1): outline.py keys that would also
  match them are pinned by line. Moving them is step 2's call, per mechanism.
