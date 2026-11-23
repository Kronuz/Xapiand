# journal-replaces-wal: journal

State: docs/harness cleanup pushed; verify jobs for it listed below. Earlier: waiting on jobs at Xapiand:0aeec8fc9 (the blob fix, the WAL text, master merged): build + group/pool tests (A1, A9) 261123-083729-a99731; gate crash 261123-083737-64aa68, no-wal 261123-083737-db63af; A8 261123-083745-3cb0e5; A2 261123-083747-1d9f09; A5 journal_replay_check 261123-083750-58484d; mutants (must fail, after a99731) d1 261123-083804-d59e5e, d2 261123-083807-24881d, d3 261123-083809-5c60e1, d4 checkpoint 261123-083811-52e8f7, d4 id counter 261123-083813-952cdc, A9 fix 3 261123-083815-599a5c, A9 261123-083817-d409c7 261123-083819-91e107, A2 truncation 261123-083820-6a878e. Also 261123-083258-b68856 (an earlier compile check at Xapiand:3d20661bd, redundant). Next: read them; fix compile errors first (a99731 builds with -k 0, so it lists them all). Blocked on 5 (stale WAL text outside the set)
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

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

Nothing open (5 answered below).

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
