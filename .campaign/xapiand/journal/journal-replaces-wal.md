# journal-replaces-wal: journal

State: waiting on jobs. At Xapiand:91113fb2f (the flush fix; the code they test is unchanged since), all -DASSERTS=ON: test_pool 261123-113325-ab914d, test_node 261123-113327-576cc8, test_loop_yield 261123-113329-ae325b, test_pool under ASan 261123-113332-95fb3d, test_shard_group under ASan 261123-113334-eca738; fails without the flush fix (data.{cc,h} as at a7f0aabd6): 261123-113337-a11289. At Xapiand:ecf684865 (storage library removed): full build, tests on, 261123-113717-eb5101. Xapiand:caa9a372e (ARCHITECTURE.md) and the runtime.svg re-render are docs only. Still queued from before: 261123-112555-9f402f (test_pool ASan at a22e1537f, before any fix: fails), 261123-101801-e33085 (test_loop_yield pre-fix diagnostic). Next: when they pass and CI at the pushed head is green, the lead submits the crash check and the full job set at that commit, without --after (the d4 checkpoint mutant takes the perl pattern of 261123-101712-4f0aa1).
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

Session 10 (2026-11-23), the lead's step (Blocked on 6 and 7, file set widened to data.* and agent-codebase.md):

1. Xapiand:09a223a4e: Data::flush copied its locators and rebuilt `serialized` in place, so after a flush each
   locator viewed the bytes it replaced (in place, while they were overwritten; freed once the string grew) or a
   pending update's holder the flush then cleared. Now the flush builds its bytes apart and `parse()`s them (feed's
   body): every locator views the Data's own bytes. Second hole, same class: a Locator moved or copied (pending's
   vector growing) kept `raw` on the source's holder, which for a short (SSO) holder is inside the moved-from
   object; Locator's copy and move now view their own holder when the source viewed its own. place_stored's
   call-site reparse (Xapiand:3fc80367f) goes: one mechanism, the flush. Tests (test_shard_group):
   `test_a_flush_leaves_its_locators_on_its_bytes` (every locator's raw inside serialize() after each flush, the
   blob bytes read back, a reparse then replace, a copy), `test_place_stored_keeps_the_other_locators`.
2. Xapiand:91113fb2f: docs/dev/agent-codebase.md describes the journal: the durability row (shard_group, command,
   the group's `app.vol-N` file set), two invariants (apply is a function of entry and starting state; prepare
   writes nothing), two traps (durable at the journal's ack, commit is the checkpoint; a locator views its Data's
   bytes). `git grep -i wal` on it is empty; `no-wal` greps src only and is unaffected.
3. Xapiand:ecf684865 (lead 2026-11-23: remove the storage library in this quest): its FetchContent block, the
   STORAGE_IO_HEADER/STORAGE_TRACE_HEADER defines, src/xapiand_storage_{io,trace}.h, the storage.h includes in
   manager.cc and database/utils.cc (outside the set; the include would not compile without the library), the
   async allowlist row. DATA_STORAGE (option, config.h.in define, the ifdefs in handler, pool, shard,
   search_views) goes; the stored-blob paths it gated always build. README and building.md option rows,
   dependencies.md (row and tree), testing.md's storage paragraph and the zstd comments in CMakeLists stop naming
   it. cmake/ and .github/ name neither. Proof: build job 261123-113717-eb5101.
4. Xapiand:caa9a372e (lead: ARCHITECTURE.md added to the set; docs only, no job): the storage-engine row and section
   describe the journal (host threads, prepare/propose, checkpoint as commit, ShardApplication, file-set blobs);
   the runtime diagram's D2 source and the background-work rule name the group loop and chore threads. Each
   storage/WAL citation: extraction row re-pointed (Kronuz/storage, done, unused); bug entries dropped: inert
   footers (storage.h:200-227), fsync barrier (storage.h:863-890), async WAL fsync (wal.cc:67), WAL replay's blob
   re-push (shard.cc:1242); LZ4-digest entry re-pointed from "WAL lines" to the LZ4 blobs data.cc still reads; gems:
   format-as-type (storage.h:232) replaced by apply-as-a-function, WAL slot array (wal.h:63) dropped. No WAL,
   storage.h or fsyncher mention is left; none was a dated record. The stale scheduler/threadpool text stays (lead's
   separate docs item).
5. assets/runtime.svg (lead added it to the set): re-rendered with `d2 --theme 0 --pad 20` from ARCHITECTURE.md's D2
   source, which gains three edges (shards propose to the group loop, committed entries to the chore threads, apply
   to the file sets) and the label "journal host"; checked from a PNG render: no WAL writers or fsyncher, nothing
   clipped. Docs only, no job.
Observed, not changed: no `Storage<>` is instantiated in src any more (manager.cc and database/utils.cc include
storage.h for nothing or its magic constants; xapiand_storage_io.h is unused), yet CMakeLists still fetches the
storage library and has DATA_STORAGE. For the lead: removing it is the WAL removal's tail (Proposed tasks).

Session 9 (2026-11-23), CI at Xapiand:a22e1537f red on 6 jobs (lead's log ~/.scratch/xapiand/lead/jrw-ci.log), Xapiand:3fc80367f:

1. The two asserts (pool.cc:358 take_turn, pool.cc:101 ChoreAwaiter) are the expected output of test_pool's death
   tests (test_a_task_holds_one_turn_at_most, test_a_chore_needs_a_scheduler print "ok" right after), on master too.
   The jobs of session 8 were RelWithDebInfo with -DASSERTS=ON, as every job of mine is.
2. heap-use-after-free in Locator::serialize (TSan, ASan, and the E2E's 616 failures; the plain E2E's "POSITIVE_INTEGER
   is not iterable" on UPDATE /assets/sample is the same corruption): place_stored set the codecs on the locators of a
   Data it had flushed and flushed again; Data::flush rebuilds `serialized` while its locators still view the old
   bytes, so the second flush read freed memory. Present since d2 (CI at Xapiand:be7dc926c shows the same 616), not
   from session 8. Fix: the codecs are set on the flushed bytes parsed again. test_pool.cc:675 (the kill -9 child
   ended by ASan's abort, not SIGKILL) is this too. Proof: ASan test_pool at head and at a22e1537f (State).
3. test_loop_yield.cc:693 (chores 142.7 ms vs on-loop 674.7 ms, 0.21 > 0.2): already at the edge before session 8 (CI
   at be7dc926c: 171.0/934.2 = 0.18; local 0.13; 0.06-0.09 in the WAL era). Cause: the group's RESTORE slice encoded
   every batch command on the HTTP loop (split_command inside propose copies each document's bytes). The chore now
   encodes each command's single entry and drops the writes' bytes; the loop proposes the entry. The targeted job
   prints the new ratio; e33085 gives this machine's pre-fix numbers.
4. origin/master merged (Xapiand:819a107b7, clean; pages.yml, AGENTS.md, agent docs).
Not the cause of any failure: the slow-commit test's chore hold and the written_key != shard_did check (the failures
above reproduce at be7dc926c, before both).

Session 8 (2026-11-23), fix round on crash check 261123-083737-64aa68 (Xapiand:0aeec8fc9, 3 of 90 failed), Xapiand:693c91e68:

1. test_pool fenced (code): the group's checkpoint threw strata::index_fenced untranslated, so error_kind() let it
   escape and the endpoint never went stale. commit_group now translates through `engine::strata::Shard::rethrow_failure`
   (rethrow_translated with the Core: errc::database, Core fenced), as the engine commit did.
2. test_node dirty (:2255), revision (:2620) and the stop order (child abort: do_commit's `current == prior + 1` at
   shard.cc:474, node_order.log in the slot) (code): the checkpoint commits the index on the group's chore and the
   engine's Core kept pending/dirty and its last own revision. `checkpoint(group, at)` now runs `at` on the chore via
   read_applied after it commits; commit_group passes `Shard::checkpointed()` (nothing owed, note() the manifest) and
   sets the endpoint's revision.
3. test_node slow commit (:737) (test): it gated the endpoint's flush chore, where the WAL-era commit ran; the group's
   checkpoint never uses it, so nothing held the commit. The test now holds the group's chore (SchedulerHold, a
   post_call that blocks the chore thread), where the checkpoint captures: same claim (the control loop stays free
   during a 400 ms commit), on the commit's real path. GateChores and A::set_chores/take_chores went with it.
4. test_loop_yield deferred==3 (code): "0 writes deferred" is deterministic, not load: restore_slice missed a write
   only when its key was gone, so k7 erased and written again on the same shard (new docid) was replaced by key. B1
   (and the engine's must_exist replace by docid) misses a document that left its placed docid; the check is now
   `written_key(term) != shard_did` (strata's replace_key keeps the key's docid, so repeats in a slice still match).
   Job 261123-101801-e33085 shows the pre-fix result at a quiet time, as the lead asked.
Job churn: the full set I queued at 693c91e68 (crash 1c2ab1, no-wal 8f25a0, reruns 8bc6da..d7f696, mutants
c0dc0e..4ba74e, 4f0aa1) was cancelled on the lead's word (targeted jobs first, the set once at the final commit). The
reruns' supersession had already cancelled 1d9f09 and 58484d at 0aeec8fc9 and the old mutants.
Observed, not changed: a later call on a group fenced earlier throws strata::group::group_fenced, which
rethrow_translated maps to errc::internal (not database), so that endpoint would not go stale by it; the first failure
already marks it stale. For the durability review.

Session 7, round 2: the mechanism notes, testing.md, the three comments say journal; status and audit records keep their wording with wal-replay.md links repointed. The git grep leaves only changelog.md:284 and options.md:418 (they state the log's removal; changelog:70 now says journal).

Session 7 (2026-11-23), docs and harness cleanup (item 5): deleted reference-guide/wal.md and mechanisms/wal-replay.md (the sidebar is generated from the content, no entry to remove); removed the WAL verb row from api.md; reworded about, basics (points at journal-durability.md), manipulation, backups, dependencies, internals, tests/coverage.md; stored_stream.sh counts `*vol-[0-9]*` files instead of `docdata.*`; search_views.h has no WAL text. Remaining hits (outside the file set, left): docs/dev/{cr1-replay,docs-site-audit,testing}.md, docs/mechanisms/{control-plane,engine-write-path,seam-fields,strata-engine}.md, docs/status/{cr1-fix,phase7c-gate-prep,phase7c-x6}.md (cr1-replay.md and cr1-fix.md link the deleted wal-replay.md), src/engine/document.h:151, src/engine/strata/document.h:149, tests/fuzz_strata_document.cc:23 (comments). In-set hits left on purpose: changelog.md:70,284 and options.md:418 (they state the log's removal).

Session 6 (2026-11-23), the lead's fix round.

1. Xapiand:3d20661bd: crash check 316fa1 did not compile: shard.cc still built a DataStorage whose header pieces
   (UUID, the Storage base) came through the deleted wal.h. Nothing in production reaches it (every index is a group),
   so the data storage goes: DataStorage and its headers, StoredUpload, upload_begin, storage_push_blobs/commit, the
   `\x00` locator metadata read, the chores parameter of do_add/do_replace/do_replace_term/do_commit; the handler's
   upload is the group's chunked command only (UploadedBlob is size, shard, upload). A pool with no journal refuses a
   stored blob (ruling 21). test_blob_io_runs_in_chores reads through the group; new test_a_stored_blob_needs_the_group.
2. Same commit, A9: test_a_reopen_aborts_an_upload_left_open failed (3847bb) because it wrote without a commit, and the
   sweep closes only a committed index (strata catalog run_close: close_ready() needs nothing uncommitted), so the slot
   never freed and the "reopen" pinned the same open entry, where the upload's final applied. The test commits before
   the close now. The A9 fix-3 mutant (599a5c) shows whether the ready point's abort is what refuses the final.
3. Xapiand:f6d8f2c65: the stale WAL text in the set (Blocked on 4 answered): config.h.in, log.h, metrics (wal errors,
   fsync gauges), README, building/options/architecture/changelog pages, strata_write_check.sh (no :wal, no WAL log
   lines, replaycrash scenario gone: its hook was the WAL's), engine_crash_check.sh (GRACE 0, ruling 23),
   graceful_shutdown_check.sh, cluster/checklist.py, coverage_e2e_server.py (no WAL verb rows), one_id_mutants.sh
   (no-replay gone), test comments, test_quarantine's names. profile_bulk.sh keeps its WAL row (ruling 22).
4. Xapiand:0aeec8fc9: origin/master merged (campaign files only). AGENTS.md not edited: agents-md has not landed.

Session 5 (2026-11-23), step e (A8). 3847bb was still running, so e went first.

1. Xapiand:9068ca7ee: the WAL (wal.*), the fsyncher, the scheduler library (FetchContent block, debouncer, stash,
   both trace headers, debounce_or_run and its test), DB_DISABLE_WAL, DB_SYNCHRONOUS_WAL, the WAL HTTP verb, the
   `wal` argument of every write and commit, `--writers` and `--wal-writer-cache-size` (now in
   removed_options_check.sh), the info keys, the fsync metrics sets and Stops 4-5 are gone. logger_fwd.h keeps
   only L_NOTHING and the L_EXC stub. test_pool's WAL tests go (the log's order is apply order by construction)
   or read the journal (FaultyIndexes keeps one; cold pin, fenced, id counter). async allowlist: the WAL rule
   and rows go, shard_group.cc's blocking sites are classified (CI 45203739416's 8 unclassified hits);
   `async_check --strict --owed` locally: 0 unclassified. run_all.sh's recovery layer runs journal_replay_check.sh.
2. Xapiand:c0dc4160d: master merged (no conflicts; strata pin stays 00eb018, which contains master's e3edb426).
3. Not done: Host::delete_group on index DELETE (Blocked on 2).

Session 4 (2026-11-23): the six jobs of session 3 were still queued (18 ahead), so the code stays unbuilt; d2's job
at head builds c, d1 and d2 together (it superseded d1's own job and mutant, resubmitted at d2).

1. Xapiand:4a117e647: the journal at `.xapiand/journal` (the lead's ruling, Blocked on 1 answered).
2. Xapiand:ed1ded9c6 (d2): no data storage with a group; `stage_blobs` compresses new stored blobs at prepare into
   CommandBlobs with placeholders at volume 0 (codec and size), `place_stored` sets them at apply; uploads stream
   as chunk entries (`upload_stored` through `next_chunked_id`/`propose_upload`), the document is the upload's
   final, proposed once (`ChunkedUpload::finished`), aborted on any failure before it; a read opens
   `app.vol-N` through the slot's directory (`read_stored`). `landed` refuses an accepted verdict with no docid
   (a final whose upload was retired). Test: test_pool `test_stored_blobs_survive_a_kill`.
3. Xapiand:6c44375c5 (d3): apply_slice with a group is `restore_slice`: a chore prepares the slice (do_replace_term
   leaves a CommandWrite, `batched`), RestoreBatcher packs, each command proposed at quorum in order, verdict ->
   `restore_results`. apply_slice/SliceWrite stay until e (the no-group path and test_loop_yield's seams use them).
   Test: `test_a_restore_slice_is_batch_commands`.
4. Xapiand:1fe793170 (d4, A9 fix 3): a request's commit with a group is `commit_group` (checkpoint on the loop,
   under the turn); the hook stays a plain strata commit. The id counter rides the write's command as set_metadata
   (`id_high_logged_`), a RESTORE slice logs it after its batches; open reads it on the chore. Ready point:
   `catch_up` reads state.open on the chore, `abort_open` proposes aborts. Tests: `test_a_commit_is_the_groups_checkpoint`,
   `test_the_id_counter_is_in_the_log`, `test_a_reopen_aborts_an_upload_left_open`.
Job churn: earlier sets (b8aa54, a03d73 and their dependents; session 3's queued c/d1/A9 jobs) were cancelled
unrun and replaced by the one set at 1fe793170 (lead: one set per head, no resubmitting per push).

Session 3 (2026-11-23): merged origin/master (Xapiand:f1944bc00, clean); strata pinned at main 00eb018d (group-catalog,
D169 (a)); chunks and restore tasks were merged by the lead (restore's three failed probes are by design, not evidence).
Built, each its own commit, none compiled locally (jobs only):

1. Xapiand:4fe9f1e91: the pin; test_group_chunk_group.cc lends its group the index (group::open_index), as a slot does.
2. Xapiand:e2f2bc9cf (A9 fixes 1-2, shard_group.cc): a chunk or final placed outside its id's term applies nothing;
   `Open::term` goes (the id names it; state bytes version 2); an abort (a final naming no blob of its upload)
   retires the upload volume. chunked_id/chunked_term move to command.h. Tests: test_shard_group
   (`test_entries_outside_their_term_apply_nothing`, `test_abort_retires_its_upload_volume`), test_group_chunk
   (the abort frees its volume).
3. Xapiand:089de4ba9 (step c): `ShardGroups` (shard_group.*): journal host over `.journal` in the data directory, the
   group loop's thread, two chore threads; DatabasePool::set_journal (the manager calls it); ShardEndpoint opens its
   `ShardGroup` last in open_blocking(directory, index) and the catalog's close_payload closes it, keeping the closed
   record. Test: test_pool `test_an_index_is_a_lone_voter_group`.
4. Xapiand:ffd0f8dd9 (step d1): `ShardEndpoint::write_document` prepares on the engine, `command_write`, proposes on the
   loop at quorum through `propose_command` (ids from the loop's term), `engine::strata::Shard::landed` checks the
   verdict and owes a refresh; set_metadata is a command; no WAL line or replay with a group; the open waits on
   `ShardGroup::catch_up` (a read_applied barrier). Test: test_pool `test_an_acknowledged_write_survives_a_kill`
   (fork, write, SIGKILL with no commit, reopen reads it); mutant 3e5477 routes writes to the engine.

Earlier sessions:

Session 2 (2026-11-21): the lead answered Blocked on 1-4 (files widened; D169 option (a), strata quest
group-catalog builds it; this quest lands after it). Merged master (cf23fbda7). Building what does not
need group-catalog:

1. CMakeLists.txt: strata pinned at main efa5214b, `strata::group` linked into XAPIAND_OBJ, faultlab
   e3fcfe3 fetched for test_pool (faultfs moved to faultlab, `kronuz::faultlab`).
2. Engine split: `engine::Shard::prepare(op, did, key, doc, args)` and `apply(prepared, doc)`; add/
   replace/replace_by_key/erase/erase_by_key are base-class wrappers over the two.
   `engine::strata::transaction_of(prepared, doc, field_table)` builds the transaction at apply against
   the applying index's table; apply passes prepare's expectation to strata (`expected_version`), so a
   stale prepare is a definite version_conflict on every replica. Test tests/test_group_prepare.cc.
3. `src/database/command.{h,cc}`: the D3 command (writes: op, did, key, expected, document bytes,
   blobs; batch for D17; schema version) and D12's chunk and final entries, `split_command`,
   `resolve_command`. Test tests/test_group_command.cc.

## Next

1. Read the six jobs and CI at ffd0f8dd9; the code was written without a build, so fix compile errors first (the
   step-c job at 089de4ba9 separates c's errors from d1's). For each job, check the log shows a complete build and
   the named test.
2. d2, blobs: storage_push_blobs becomes CommandBlob records compressed at prepare with placeholder locators;
   storage_get_stored reads `app.vol-N` through the group's file set (on the chore, or a reader of the file);
   upload_stored/index_uploaded take propose_upload/propose_upload_final (the handler.cc SEAMs); DataStorage and
   StoredUpload go. A2's crash evidence (blob_crash_check.sh, the `crash` check) on that build.
3. d3, RESTORE: DocIndexer::apply_shards feeds RestoreBatcher through RestorePropose (each command once the
   previous is placed); Shard::apply_slice and SliceWrite go; journal_replay_check.sh on that build (SETTLE=0).
4. d4: commit() awaits group.checkpoint() on the loop from a request (never from a commit hook); id_high moves into
   the log (a set_metadata command at issue, or ShardGroupState).
5. A9 fix 3: after catch_up in open_blocking, the leader proposes abort_entry(id) for each id in state().open; test:
   a close with an upload in flight resumes its term at reopen and the abort retires it.
6. e: written (session 5); Host::delete_group on index DELETE waits on Blocked on 2. Read the step-e jobs (State).

Done (session 2's plan, kept for reference):
2. c. Build against group-catalog (FETCHCONTENT_SOURCE_DIR_STRATA=/Users/kronuz/code/Kronuz/strata-wt/
   xapiand/group-catalog, strata:096dbd04): its `group(loop&, config, jn::IO& directory, index_type& index,
   App&, chore)` borrows the slot's index. One `strata::group::loop` per reactor loop and one
   `journal::Host` per data directory in manager.* (restore task holds manager.*: coordinate, or the
   lead moves the loop/host setup into pool.*); the catalog slot constructs
   `group<ShardApplication>` over its index and IO.
3. d. ShardEndpoint writes: prepare under the turn (engine prepare), the blobs moved from
   storage_push_blobs into CommandBlob records (compressed at prepare, placeholder locators), then
   `split_command` and `propose`; ack levels via `propose(level)`; the blob read path for app. volumes
   (storage_get_stored); commit() posts a checkpoint to the group's chore (msg D169 revised: never awaited
   from a commit hook); open_blocking's WAL replay goes. id_high: from the index at open, or into
   ShardGroupState (decide at d).
4. e. Delete the WAL (wal.*, fsyncher, DB_DISABLE_WAL / DB_SYNCHRONOUS_WAL and the four tests' flag,
   options), `no-wal` check; merge racing-put-409 and resolve it in prepare_write's replace case.

## Messages

- Lead 2026-11-21 (group-catalog's contract change, D169 revised): the catalog's commit hook never
  sync-waits on `group.checkpoint()` (commit hooks run inside chore_scope; the group's chore takes the
  index's write lease, which waits while the chore bit is held: deadlock on any lane). The commit stays
  a plain strata commit; the group's checkpoint runs at close through the new catalog hook
  `close_payload`, outside the lease, before index.close(). So step d: `commit()` posts a checkpoint to
  the group's chore (periodic or explicit), never awaited from a commit hook. Push once per round of
  code, when only jobs remain (a push cancels the branch's CI run).

- Lead 2026-11-21 (racing-put-409 lands first in src/engine/strata/shard.cc, Xapiand:01ec326b6 and
  Xapiand:cc8c27362): a replace whose key sits at another docid is `version_conflict(found->second)`, not
  `invalid`. Not applied ahead (test_engine_strata.cc, outside my set, still expects `invalid` until it lands):
  when master brings it, the merge conflicts in Shard::replace; resolve it in prepare_write's replace case.

## Proposed tasks (posted to the lead)

The quest's own table holds: `core` (me) first; then `chunks` (handler.*, tests/test_group_chunk*,
harness/blob_crash_check.sh) and `restore` (manager.*, tests/test_wal_replay.cc, the three WAL
harness scripts, profile_bulk.sh) in parallel, disjoint files; then `levels` (opts.*,
search_views.cc, the request tests, power_loss_check.sh, run_all.sh, docs). One addition, in
strata: `group-catalog` (item 4), which `core` step c waits on.

## Blocked on

6. (2026-11-23) Outside the file set, left: docs/dev/agent-codebase.md (new from master's agents-md) still describes
   the WAL (lines 14, 71-72, 76, 80: `src/database/wal.*`, WAL replay, WAL durability). Recommend: add it to the set
   and I reword it to the journal, as item 5's files were.
7. (2026-11-23) Outside the file set: Data::flush (src/database/data.cc) leaves its locators' `raw` views in the bytes
   it replaced, the root of item 2 of session 9; place_stored no longer touches locators after a flush, but any caller
   that does is exposed. Recommend: add src/database/data.* and have flush feed() its new bytes at its end (one
   parse), with a unit test that flushes twice under ASan. Until then, nothing in the quest depends on it.

9. (2026-11-23, done in session 10 item 5) Outside the file set: assets/runtime.svg is rendered from ARCHITECTURE.md's D2 source, which now
   shows the journal threads; the SVG still shows the WAL writers and fsyncher. Recommend: add it and I re-render
   (`d2 --theme 0 --pad 20 <source>.d2 assets/runtime.svg`), or the lead's docs item takes it.
8. (2026-11-23, done in session 10 item 4 after the lead widened the set) Outside the file set: ARCHITECTURE.md still describes the storage library and the WAL as current
   (the Storage engine row and section, `storage.h` citations, the "Haystack-style store" extraction row, three
   storage/WAL bug entries, the format-as-type gem; 17 WAL mentions). It reads as a dated close-read review, but
   its subsystem table and sections are descriptive. Recommend: widen the set and I rewrite the storage-engine row
   and section to the journal and drop the removed code's bug entries, or a docs quest refreshes the whole file
   (its scheduler and threadpool text is stale too, as is dependencies.md's tree on master).

Items 6 and 7 are done (session 10). Earlier items are answered below.

Lead rulings, 2026-11-23:
- 2: option (ii). Index DELETE is its own quest, index-delete (after strata-journal-pin bumps strata's journal pin
  past journal:131eb2e, and after this quest). 501 is today's behavior on master, not a regression here.
- 3 and 4: the file set now covers the three deleted files and every file in item 4. Removing the WAL means
  removing it everywhere (code, options, metrics, docs, harness); do item 4 in this quest. AGENTS.md is shared
  with agents-md, which lands first: merge master before editing it.
- 5 (2026-11-23): the set now covers every file item 5 lists, plus docs/astro.config.mjs and docs/src/sidebar*
  for the page's sidebar entry. Delete wal.md and wal-replay.md; fix the rest in this quest.

## Evidence

Jobs prove criteria by their tags; paste `campaign brief`'s generated table here when they finish.

| Criterion | Evidence |
|---|---|
| A1 | partial: 261121-105434-07ffd7 (mutant: apply without prepare's expectation fails test_group_prepare); the group, WAL deletion and no-wal pending |
| A3 | partial: 261121-105359-853976 (codec, application, prepare tests), 261121-105428-7308c6 (mutant: a command proposed whole fails test_group_command), 261121-105431-21e0e8 (mutant: no lower-term retirement fails test_shard_group); propose path and the upload's error answer pending (chunks task) |
| A9 | 261123-042848-b46902 (plain, at e2f2bc9cf); fails without: 261123-042851-e13216 (a chunk applies outside its id's term), 261123-042854-5886cb (an abort seals its volume); fix 3 pending (d) |
| A1 (c, d1) | 261123-043412-42a61c (step c: build, test_pool); 261123-044042-0153c6 (d1 tests); fails without: 261123-044046-3e5477 (writes skip the group, the kill -9 case fails); WAL deletion, no-wal and CI pending |
| A2, A4-A6 | none yet |
| A7 | draft: docs/mechanisms/journal-durability.md (built parts) |

## Result

<When ready for review.>

## Rulings I made

Drafts, applied when the code lands; each names the contract it shapes (format-freeze reviews them).

1. Blob volumes are the group's file-set files (`app.` prefix) in a positional format of their
   own, not the storage library's block framing: D13 converts no pre-release directory, and
   `Storage::write` appends where the file ends, which D3's retry-from-start forbids
   (`append-on-retry`). Contract `xapiand:volume-file-set`.
2. A blob's compressed bytes are made at prepare and carried in the command; apply writes them
   verbatim. Then a locator's offset is a function of the log even across replicas built with
   another zstd. Contract `xapiand:command`.
3. The command carries `engine::Document::serialize()` bytes as D3 says; the strata transaction
   is built at apply against the voter's own field table, so a field declared by an earlier entry
   resolves the same on every voter.
4. The task pin moves to strata's own (95798a02): strata::group's journal and consensus headers need
   task's blocking_scheduler, which Xapiand's 18eb5167 lacks. A consequence of the strata bump the lead
   assigned (CMakeLists.txt is in the set); CI tells whether anything else in Xapiand depended on 18eb5167.
5. faultlab is fetched for test_pool only, at strata's e3fcfe3 (strata declares it only as top level).
6. `engine::PreparedWrite` (not `Prepared`: engine/search.cc has a `Prepared` query class). Its
   `expected` is always set by prepare for replace and replace_key (the leader's expectation, D3), the
   key's version for erase_key, and none for erase by id (strata checks versions only by key; prepare
   checks it). `WriteOp::none` and `replayed` stay (revised, session 5): they serve any versioned document's replay, and their
   tests are in test_engine_strata.cc outside my set; removing them is a follow-up (Blocked on 4).
7. A batch's transaction declares a new field once: a later write of it names the earlier declaration
   (the table keeps the first document's kind, as sequential transactions would).
8. Volumes rotate past 32 GiB (`VOLUME_BYTES`, master's data-storage volume bound, storage.h); locators keep
   byte offsets. Chunked-command ids must be unique across the group's log (the proposer puts its term in
   the high bits); a chunk of an id open in a lower term is dropped, and its final writes nothing.
9. The command codec is LEB128 over the kinds command/chunk/final with a version byte (pre-release bytes,
   contract `xapiand:command`/`xapiand:chunked-command`, format-freeze reviews them); `split_command`
   stages the largest in-place bytes first until the final fits.
10. The journal's IO completes inline (InlineCompletion) on the group loop's own thread, never a reactor's:
    correctness first; T11 measures whether the loop's syncs sit on the write's critical path.
11. A group's id is the first 8 bytes of its index's uuid (nonzero), its journal identity the uuid: the id
    travels with the index directory; a copied directory's second open is refused by the loop's table.
12. One journal host per data directory (`.journal`), two chore threads, a group on chore `id % 2`; closed
    records kept per process (a restarted process recovers with an election).
13. A write holds the turn across prepare, propose and the quorum answer: prepare reads the index as the previous
    write left it. One write per index in flight; T11/Q14 decide pipelining.
14. A9's rf > 1 cases are tested on the replica's application entry by entry (the deposed proposer's chunk
    placed in the new term): apply is a function of entry and state, so it is the sequence every voter applies.
15. A request's commit is the group's checkpoint (d4); the catalog's commit hook stays a bare strata commit (D169).
16. A stored blob's record in `app.vol-N` is its bytes, zstd-compressed at prepare only when smaller; the locator's
    type names the codec (`stored` / `zstd_compressed_stored`) and its size is the record's. Uploads are raw.
    Contract `xapiand:volume-file-set`.
17. A document bound for another shard than its upload's group is refused before anything is proposed
    (ClientError), not applied then reported as master did.
18. RESTORE's commands are proposed one after another, each awaited at quorum; D17's pipelining waits on T12.
19. The crash check's command passes `build/bin/xapiand`; blob_crash_check.sh accepts it as the build's binary.
20. The crash check (lead's command) configures without -G Ninja; the lead's to fix.
18. Removing the WAL removes its user-visible surface with it (the quest's outcome): the `:wal` HTTP verb, the
    `wal` info keys and metadata key, `--writers` and `--wal-writer-cache-size` (removed options, unknown arguments).
19. A pool with no journal (tests, the CLI) is durable only at its commits; no second log replaces the WAL there.
20. The mutant probes carry `--for` their criterion with `--fails-without`: the tool refuses `--fails-without`
    with neither `--for` nor `--task`, and the lead's rule forbids `--task` for probes.
21. A pool with no journal (tests only; the manager always sets one) holds no stored blob: a write naming one, an upload
    or a batch is refused with `invalid` before anything applies, and a stored read raises `invalid`. The data storage
    it used was the WAL era's second blob path; the group's file set is the one path (D16).
22. `harness/profile_bulk.sh` keeps its WAL layer: T11 profiles master's WAL build as the baseline the journal is
    measured against.
23. `harness/engine_crash_check.sh` defaults GRACE to 0: an acknowledged write is in the journal before its answer, so
    none may be missing after kill -9 (D2).
24. The `\x00` locator metadata read (`GET /index/:<locator>`) is removed with the data storage it read.
25. A group checkpoint leaves the engine as its own commit did: the Core owes nothing and holds the manifest's revision,
    set on the group's chore (read_applied after the checkpoint), never from the reactor while the chore may apply.
26. test_node's slow-commit case holds the group's chore instead of the endpoint's flush chore (the WAL-era commit's
    path, which no commit takes now); its claim is unchanged.
27. Data's invariant is that after construction and every flush each locator views the Data's own bytes, owned by
    Data::flush (it parses what it built); a pending Locator views its own holder or the caller's memory
    (`data(string_view)`), and a copy or move keeps that. Callers never reparse to work around a flush.
28. The codebase guide drops the storage library's write-cursor invariant and footer trap with the WAL's: no
    Storage<> volume is written in src now (the blob volumes are the group's file set).
29. DATA_STORAGE goes with the library (lead's instruction): it no longer selected a store, only whether stored blobs
    build; every index's group serves them, so there is one build shape. config.h stays included in pool.h and
    shard.h (comment dropped): about twenty files read XAPIAND_* macros through transitive includes, and a lost
    one fails silently.
30. database/utils.cc (outside the set) loses its storage.h include and a history comment: required for the build
    once the library is gone.
