# journal-replaces-wal: journal

State: waiting on jobs 261123-042848-b46902 261123-042851-e13216 261123-042854-5886cb (A9, at e2f2bc9cf), 261123-043412-42a61c (step c, at 089de4ba9), 261123-044042-0153c6 261123-044046-3e5477 (step d1, at ffd0f8dd9) and CI at ffd0f8dd9; steps c and d1 written blind (no local build), next d2-d4
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

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
6. e: delete the WAL, fsyncher and scheduler (A8), DB_DISABLE_WAL/DB_SYNCHRONOUS_WAL; Host::delete_group on index
   DELETE; the no-wal check; then the full `crash` check once.

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

1. `.journal` (the journal's directory in the data root) is also a valid index name: index_path::valid accepts a
   leading dot, so a user index `.journal` would nest under the journal host. Options: (a) refuse creating an
   index whose first segment is `.journal` (a reserved name, documented); (b) name the journal with a byte
   valid() refuses; (c) leave it. Recommend (a); it changes user-visible behavior, so the lead decides. Work goes
   on meanwhile.

Answered earlier: nothing else open. Session 1's items 1-4 were answered by the lead on 2026-11-21: the file set widened
(CMakeLists.txt, engine shard, command.*, shard_group.*, the DB_DISABLE_WAL tests, search_views.cc);
item 4 is D169, option (a), built by strata quest group-catalog, which this quest lands after.

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
   checks it). `WriteOp::none` and `replayed` exist only for the WAL's replay and go with it (step e).
7. A batch's transaction declares a new field once: a later write of it names the earlier declaration
   (the table keeps the first document's kind, as sequential transactions would).
8. Volumes rotate past 32 GiB (`VOLUME_BYTES`, master's data-storage volume bound, storage.h); locators keep
   byte offsets. Chunked-command ids must be unique across the group's log (the proposer puts its term in
   the high bits); a chunk of an id open in a lower term is dropped, and its final writes nothing.
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
15. A request's commit and the catalog's commit hook stay bare strata commits (D169) until d4.
9. The command codec is LEB128 over the kinds command/chunk/final with a version byte (pre-release bytes,
   contract `xapiand:command`/`xapiand:chunked-command`, format-freeze reviews them); `split_command`
   stages the largest in-place bytes first until the final fits.
