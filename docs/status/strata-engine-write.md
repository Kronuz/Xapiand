# strata-engine-write (S1, phase 7a)

Branch `phase7a/strata-engine-write` from Xapiand master `eb83813a8`. Local only, not pushed, no CI run.
Design: `strata/docs/design/xapiand-on-strata.md` (strata:9dc5fd3), row S1, D2 to D13, gate rows T6 to T11.

## State

Code complete and proven locally, sanitizers included; not reviewed. Strata and prospect are
pinned at prospect `9de01a18`, which pins strata `75d1e7e` (format 14 step 1, 296 commits behind strata main);
Xapiand's compressors pin moved to strata's `816bc82`.

## Done criteria (design row S1)

1. Lifecycle: open, create, close (the last handle commits, as a Xapian writer does), reopen, commit,
   transactions (cancel reopens from the last commit). Done.
2. add, replace, erase, by docid and by key. Done.
3. Version check-and-set from strata's version map, no `V` terms, no version slot. Done.
4. Metadata get, set (empty clears), keys. Stats: uuid, revision, lastdocid, doc count, total length. Done;
   the doc-length bounds are 0 until prospect P4.
5. `DocView`: did, version, data. Values are TODO-gated on P1 (`phase4/values-bytes`, not landed):
   `Document` holds them and the WAL line carries them, the shard stores none, `DocView::value` is empty
   (`values_gap` in the unit test flips when P1 lands). `terms`, `term_details`, `serialise` of a view raise
   `not_available` until P2.
6. WAL replay through `Document::unserialise(kind, ...)`: done, through a node (below).
7. Hits by docid: `get(did)` and `scan(from, sink)`. Done.
8. Engine per index: `engine::kind_of(path)` (the files on disk, else `XAPIAND_ENGINE`, registry always Xapian).
   The saved `IndexSettings` field of D8 is not done (see Next).
9. `harness/seam_check.sh` unchanged (only `src/engine/strata/` names strata or prospect); the other
   `Xapian::` counts did not move.

## Done

1. Xapiand:75178826d the engine, `Document(Kind)`/`set_key`, `kind_of`, `open_local(kind, ...)`.
2. Xapiand:b078d5a18 tests on both engines, the codec fuzz target, the pins, the harness.
3. Xapiand:91da13d05 the `:wal` dump no longer dereferences a null shard (found by the harness).
4. Proof.
   1. `ctest` 21 of 21 (`test_engine_strata`: 8081 checks, the shared expectations on both engines, the
      strata-only ones, the codec's damaged-input loop).
   2. Node (`harness/strata_write_check.sh`): PUT, GET, replace (version 2), DELETE 204, the index is a
      strata directory with no glass files; 30 PUTs without a commit, `:wal` of every shard decoded, kill -9,
      restart, a write to every shard replays the log (`Read and execute operations WAL file ('sw/...`),
      all 30 served, second crash loses nothing, a write between crashes is replayed. PASS.
   3. Reverts and mutants. (a) The `:wal` fix reverted: the node segfaults on `GET /sw/.__N/:wal` and the
      harness fails ("answered for 0 of 5 shards"). (b) The live count read from the manifest (before the
      fix): 3 checks of `test_engine_strata` failed (`doc_count` 0 before a commit). (c) Mutant, replace by
      key bumps the version by 2: caught, 2 of 8028 checks fail, by the engine's own
      id-and-version cross-check and by the version expectations.
   4. Fuzz: `tests/fuzz_strata_document.cc` (libFuzzer, Homebrew LLVM, ASan+UBSan), 808144 runs in 61 s,
      no finding. Build: `clang++ -std=c++20 -fsanitize=fuzzer,address,undefined -Isrc tests/fuzz_strata_document.cc src/engine/strata/document.cc`.

   5. ASan+UBSan build (Apple clang, `-DASAN=ON` and `-fsanitize=undefined`): `test_engine_strata` 8081 of 8081;
      `strata_write_check.sh` PASS (NDOCS=10, ASan is slower than the 3 s autocommit); a 3000-document RESTORE
      on strata and on Xapian. No ASan report; the only UBSan lines are the known third-party ones (`lz4.c` zero
      offset to null, rapidjson `stack.h`), none in `src/engine`, the shard, the WAL or the handler.
   6. Bulk screen (not a measurement: a timed `RESTORE` of 20000 documents, 6 words of title and 40 of body, one
      `--solo` node per engine, 3 interleaved rounds under `quiet strict`, RelWithDebInfo without LTO;
      `harness/profile_bulk.sh` is not on master, so the engine's own share is not isolated):

      | Engine | docs/s (3 rounds) | server CPU s |
      |---|---|---|
      | Xapian | 4608, 4595, 4595 | 15.5, 16.0, 15.7 |
      | strata | 5200, 5185, 5363 | 12.3, 12.3, 12.0 |

      The whole pipeline is 1.14 times faster in docs/s and uses 0.77 of the CPU. strata does less work here (no
      values, no term lists, no spelling), so this is a screen for the T11 run, which needs the engine share from
      `profile_bulk.sh` on both binaries and the same work on both sides.

## Findings

1. **Key prefix `Q` cannot be the index's key prefix.** strata refuses any term that starts with the key
   prefix, and Xapiand's field terms are `name.word`, so a field called `Quote` would make every such
   document invalid. The index key is `\xff` plus the id term (no UTF-8 string starts with it); the id term
   stays an ordinary term. This departs from D5's "key prefix `Q`"; the architect should rule (it changes
   nothing for Xapian).
2. **Docid reads need a strata API.** The core has no public get-by-docid or find-docid-version (`locate_docid`
   is private), so `get`, `erase(did)` and `replace(did)` of a key-less-at-prepare document flush the write
   buffer (`refresh`) and walk the segments. A public `find_docid` and `get_docid` in strata makes them one
   lookup and removes the refresh. WAL replay does not pay it (the replayed id is above `last_docid`, or the
   document carries its key).
3. **WAL replay is lazy for strata too.** A readable open does not replay; the next writable open does, as
   with Xapian. A strata shard exists (manifest 1) as soon as it is created, so unlike an uncommitted new
   Xapian shard, a read after a restart sees the empty committed state until a write opens the shard.
4. **Dependency pins.** prospect's pin is strata `75d1e7e`; strata main has moved 296 commits (shared readers,
   cold tier). Xapiand now builds that pair. Moving to strata main means moving prospect's pin first.
5. Documents without a key are refused (a version belongs to a key in strata); every Xapiand document has one.
6. A strata handle's reads see every write at once (D7); a Xapian readable sees only committed data, so
   `harness/strata_write_check.sh` is not meaningful on Xapian.

## Next

1. T11 on `profile_bulk.sh` once it is on master, with values (P1) and term lists (P2) on the strata side.
2. Merge policy and the executor: strata merges inline on the committing thread (`auto_merge`, no background
   executor); bind its executor to the reactor's pool (D13's adapter) with S2.
3. D8's `IndexSettings` field (saved with the index, a REST setting) instead of the process default.
4. Values when P1 lands (`values.h`, the `values_gap` test), term lists when P2 lands (`DocView::terms`).
5. `wal_replay_check.sh` and `wal_replay_agg_check.sh` assert through search and aggregations (S2, S3), so
   they cannot run on a strata index yet; `strata_write_check.sh` is their write-slice counterpart.
6. Strata search: `engine::search` raises `not_available` for a strata shard (S2).

## Blocked on

Nothing.
