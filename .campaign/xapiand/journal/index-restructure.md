# index-restructure: journal

State: waiting on jobs 261121-221903-f4df46 261121-221904-8d6ee6 261121-221906-8a1ce9
Quest: .campaign/xapiand/quests/index-restructure.md (the lead's; never edit it)

Code: strata worktree /Users/kronuz/code/Kronuz/strata-wt/xapiand/index-restructure, branch
quest/xapiand/index-restructure. Step 1 (the move) at strata:a7448b3f.

## Doing

Step 1 done and committed; its evidence jobs are queued.

## Next

1. Read the three jobs. build-test or selfcontained red: fix the mover (not the output by hand),
   rerun it over main's index.hh, recommit. fnsizes red: explain each changed row here (A2).
2. After split-check-members lands: rerun `members` (`campaign verify index-restructure`) and
   answer each warning here.
3. After turn-log-parity lands (lead's call): merge main, restore main's index.hh, rerun the mover
   (`move_members.py apply include/strata/core/index.hh .scratch/index-restructure/map.txt`), map
   any new member, re-verify.
4. Step 2 (subobjects), from the candidates below, once step 1 lands.

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

Nothing. A2's `members` half waits on quest split-check-members (not landed); its job
261121-221904-5a5074 fails until then (split_check has no --members): expected, not evidence.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-221903-f4df46 at strata:a7448b3f (queued); CI after push |
| A2 | members: pending split-check-members (261121-221904-5a5074 fails until it lands). fnsizes: 261121-221906-8a1ce9 (Release+LTO test binaries, main cc30217f against head, fails on any size change; queued). Mover self-check: all 196 moved bodies, re-indented one tab, are byte substrings of main's index.hh; every line added to index.hh is a declaration ending in `;`, an include or its comment |
| A3 | step 2, not started |
| A4 | strata docs/source-layout.md, docs/mechanisms/index-layout.md (strata:a4a82ce6) |

Also: selfcontained.sh over index.hh and index_*.hh: 261121-221904-8d6ee6 (queued).

## Result

<Step 1 pending its jobs.>

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
  (git add -f) for the rerun after turn-log-parity; drop them before landing.
