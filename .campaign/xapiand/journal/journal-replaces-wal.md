# journal-replaces-wal: journal

State: blocked on lead: file set (CMakeLists.txt, src/engine/strata/shard.*, new src/database files) and a strata task (group over the catalog's index)
Quest: .campaign/xapiand/quests/journal-replaces-wal.md (the lead's; never edit it)

## Doing

Session 1 (2026-11-21): survey of master at ce5e4df02 against strata main efa5214b (group-core
landed at strata:51dcff1e). No code yet: every core step needs a file outside the set or a strata
change (Blocked on). The plan below is concrete enough to start the moment the set widens.

## Next

1. Lead: widen the file set and pin strata (Blocked on, items 1-3); launch the strata task (item 4).
2. Core, in this order, each step committed with its test (tests/test_group_*.cc):
   a. `src/database/command.{h,cc}`: the D3 command codec (op, id/key, expected version, schema
      version, `engine::Document::serialize()` bytes, blobs already compressed), D12's chunk and
      final entries; round-trip and refusal tests (`test_group_command.cc`).
   b. `src/database/shard_group.{h,cc}`: the application (`strata::group::application`):
      `effects` writes blob records positionally into `app.` volumes from the state's
      (volume, length); `transaction` builds the prospect transaction from the document at
      apply against the index's field table (needs engine/strata's `fill`, exported);
      `advance` moves (volume, length, id_high, open chunked commands); `encode`/`decode`.
      A lone-voter test over a temp directory: PUT, blob, delete, metadata, version conflict,
      crash and recover from the bundle (`test_group_app.cc`).
   c. One `strata::group::loop` per reactor loop and one `journal::Host` per data directory
      (`<data>/.xapiand/journal`), in manager.*; the catalog slot opens the group over its index
      (needs item 4).
   d. ShardEndpoint's writes become prepare (under the turn, from applied state plus own
      unapplied proposals) then `propose`; `commit()` becomes `group.checkpoint()`; the
      autocommit debouncer becomes the checkpoint cadence; open_blocking's WAL replay goes.
   e. Delete `src/database/wal.*`, `src/xapiand_fsyncher.h`, `DB_DISABLE_WAL`,
      `DB_SYNCHRONOUS_WAL`, `--num-async-wal-writers`/`--num-fsynchers`; `no-wal` check.
3. Then the tasks below (chunks, restore, levels).

## Proposed tasks (posted to the lead)

The quest's own table holds: `core` (me) first; then `chunks` (handler.*, tests/test_group_chunk*,
harness/blob_crash_check.sh) and `restore` (manager.*, tests/test_wal_replay.cc, the three WAL
harness scripts, profile_bulk.sh) in parallel, disjoint files; then `levels` (opts.*,
search_views.cc, the request tests, power_loss_check.sh, run_all.sh, docs). One addition, in
strata: `group-catalog` (item 4), which `core` step c waits on.

## Blocked on

The lead, for files and a strata task. Messages posted 2026-11-21 (see `campaign brief`).

1. `CMakeLists.txt`: pin strata at or past strata:51dcff1e (main is efa5214b; master and
   strata-pin-catalog's e3edb426 both predate group-core) and link `strata::group` into
   `XAPIAND_OBJ` (it brings consensus's headers; consensus and journal are header-only). The
   lead's `campaign pin --push` plus one line in `target_link_libraries(XAPIAND_OBJ ...)`.
   The bump also breaks `tests/test_pool.cc:75` (`strata/core/faultfs.hh` moved to faultlab at
   strata:afda6e61; my set holds the file, I fix it once pinned), and Xapiand's own workpool pin
   (406d2f37, CMakeLists.txt:538) precedes the one strata main declares (640c66bf); CI tells.
2. `src/engine/strata/shard.{h,cc}` and `src/engine/shard.h`: the engine checks versions,
   finalises, stamps and builds the prospect transaction inside one call that also applies it
   (`replace_by_key` -> `fill` -> `apply_t`). The group splits that: the leader prepares
   (version check, finalise, stamp) and every voter builds the transaction at apply against its
   own field table (`fill` reads `table.mark()`). Needed: an exported prepare and
   `transaction_of(document, index)`; the replay paths (`replayed()`, `replay_applies`) go.
   No active quest holds src/engine/strata/** (only `release`, blocked, with `**`).
3. New files `src/database/command.{h,cc}` and `src/database/shard_group.{h,cc}`
   (`tests/test_group_*.cc` is already in). The four tests that name `DB_DISABLE_WAL`, which the
   flag's deletion breaks: `tests/test_cheap_reopen.cc:56`, `tests/test_endpoint_uuid.cc:335`,
   `tests/test_loop_yield.cc:1173`, `tests/test_node.cc:1154`. And `src/server/search_views.cc`
   (RESTORE's `DB_DISABLE_WAL` at :1726, the WAL dump view at :1833, `num_fsynchers` at :804),
   already expected once restore-stream lands.
4. strata: `strata::group` opens and owns its own index (`group.hh:835`,
   `ix_.emplace(index_type::open(*dir_, ...))`), while Xapiand's catalog owns each index by value
   (`catalog.hh:347`, `open_index` returns `Index`; ShardEndpoint borrows it, x-pool D2). Two opens
   of one directory cannot coexist (strata locks it). D11 says the catalog owns both, and
   group-core's A6 named it, but its mechanism note says "the catalog's ownership follow[s]" and
   no branch builds it. Options:
   (a) recommended: a strata task `group-catalog`: `group::config` takes the slot's index
       (borrowed; recover() uses it, close() leaves it to the catalog), and the catalog's hooks
       for a group's index run the group's checkpoint as its commit chore and the group's close
       before the index's, with the group's chore on a lane other than the catalog's transitions
       (a sync wait from the commit chore on the group's chore would otherwise deadlock). About
       150 lines plus a test that a catalog close of an idle group publishes a bundle covering
       applied (group-core A6's clause).
   (b) Xapiand keeps groups outside the catalog: a second lifetime and idle mechanism, which D11
       rejects.
   (c) Xapiand wraps a group as the catalog's `Index` type: about 20 forwarded methods
       (`lease`, `fenced`, `uncommitted`, `reclaim`, `demote`, ...), a special case.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.

| Criterion | Evidence |
|---|---|
| A1-A7 | none yet |

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
