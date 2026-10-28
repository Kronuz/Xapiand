# strata-engine-write (S1, phase 7a)

Branch `phase7a/strata-engine-write` from Xapiand master `eb83813a8`. Local only, not pushed, no CI run.
Design: `strata/docs/design/xapiand-on-strata.md` (strata:9dc5fd3), row S1, D2 to D13, gate rows T6 to T11.

## State

Code complete and proven locally, sanitizers included (proof round 2 closed the auditor's gaps: replay idempotence,
concurrency, crash windows, mutants). Code review: approve with conditions (`docs/reviews/strata-engine-write-code.md`);
the fix round below folds in CR1 to CR7, awaiting the re-check. Strata and prospect are
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
4. Xapiand:9cbd16f51 idempotent replay (the version stamp), `CRASH_POINTS`, the S1 proof tests (concurrency, reads
      by id, commit on drop, not_available); Xapiand:a73ea8d18 the crash scenarios and the model client.
5. Proof.
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

   7. Proof round 2 (auditor gaps).
      1. Replay idempotence. Cause: the strata index assigns a version as stored + 1 and cannot be told one, and the
         log line did not carry the version, so replaying a line the index already held bumped it (the unit test
         asserted version 2 and its comment said the opposite). Fix (Xapiand:9cbd16f51): the shard stamps the written
         version into the document before the log serialises it; a document read back with a stamp is applied only when
         the index is behind it. `replay` now asserts version, data and doc count unchanged over repeated replays, an
         older line after a newer write, and the same by key. With the skip removed 21 checks fail (mutant
         `replay-skip`); with `>=` weakened to `>` 19 fail. At the node the double apply cannot arise by itself (pending
         writes die with the process, so a replay starts from the committed state, and `execute_line` compares the
         manifest revision), so this is the engine's own contract; a node mutant that applies each replayed line twice
         is caught by the `versions` scenario (3 assertions fail, 18 of 30 documents right). Xapian is unchanged: its
         seam versions every replace the same way, and its test keeps asserting version 2.
      2. Concurrency: `concurrent_cas` (8 threads, 3 hot keys, 150 wins each): every winning version appears exactly
         once and the versions that won are 2..final, the final data is the last winner's, conflicts occurred; 20 rounds
         of 8 creators of one new key (exactly one wins); 8 erasers at one version (exactly one). TSan build
         (`build-tsan`, Homebrew LLVM, own `_deps`): 9482 checks, 0 reports. Mutant removing the lock in `replace`:
         TSan reports data races (44 lines) and the plain build aborts. ASan+UBSan build (`build-asan`, Homebrew LLVM, own
         `_deps`): 9482 checks, 0 ASan reports, 0 UBSan lines. Homebrew clang needs the SDK's zlib and uuid headers
         linked into a scratch include dir (the SDK `usr/include` as `-isystem` breaks its libc++) and rapidjson 1.1.0's
         `GenericStringRef::operator=` made `= delete` in the build's own copy.
      3. Crash windows (`harness/strata_write_check.sh versions|commit|replaycrash`, model client
         `harness/strata_write_model.py`): every document is written as `{seq: k}`, k the version the write must
         produce; verification checks `_version` == `seq` == acknowledged for every id, deleted ids are 404, and an
         unanswered write may be either side. `commit`: the node kills itself (`-DCRASH_POINTS=ON`,
         `XAPIAND_STRATA_CRASH=point:n`) before the commit, after the segment flush before the manifest, after the
         commit, at the 3rd pass (mid-way through the five shards), under random puts and deletes; restart, replay,
         verify, crash again, verify: PASS for all three. `replaycrash`: kill -9, then the node dies mid-replay at the
         5th replayed line, restarts and dies at the 2nd, then converges (40 of 40), then one more crash (40 of 40).
         `versions`: v1, v2, v3, delete, re-create, crash, replay, second crash, more writes, crash: all correct.
         A node mutant that skips the commit (`commit-is-lost`) survives: the WAL keeps the whole history and replays it,
         so a lost commit is invisible to the model by design (durability is the WAL's).
      4. Mutants, all in `tests/test_engine_strata.cc` unless noted: commit-on-drop removed (`~Core`): killed (`strata
         only`, `commit_on_drop`); `apply` not marking dirty: killed (`shared`, `replay`, `fresh_reads`); `refresh` a
         no-op: killed; `locate` without its refresh (a stale read-by-id): killed; key-owner check removed: survived
         until the test asserted the shard's own message (the index refuses the write too, with its words); now killed;
         `check_version` removed from `replace`: killed (`concurrent_cas`: no conflicts, plus `shared`); no lock in
         `replace`: killed. `erase` not marking dirty survives and is equivalent: an erase of a flushed document shows at
         once through the segment's live bits, and an erase of a buffered one flushes first in `locate`.
      5. `not_available` for `DocView::terms`, `term_details`, `serialise`, and for `engine::search` (a term and
         match-all) on a strata shard (`strata_only`).
      6. Fuzz: `fuzz_strata_document` now also checks the stamp round trip; 443926 runs in 61 s, no finding.
      7. `docs/mechanisms/strata-engine.md` describes the stamp and the crash points.

   8. Review fix round (Xapiand:d0a664b8c; CR1 to CR7; ASan+UBSan and TSan builds in separate dirs with their own `_deps`,
      `test_engine_strata` 9549 checks and `test_engine_replay` 17 pass in both, 0 sanitizer reports; the reviewer's
      probes `conc` (806 checks) and `oldline` (20001) pass in both).
      1. CR1: a `Core` whose index is empty (a failed `cancel_transaction` reopen) raises `errc::database` from every
         entry point (`Core::check`), never touching freed state; `reopen()` or the next `acquire` retries the open.
         `cancel_reopen_fails` (chmod 0 on the strata dir). With `check()` disabled the test dies on the ASan
         heap-use-after-free the reviewer saw.
      2. CR2: `~Core` does not commit while a transaction is open. `drop_mid_transaction` runs on both engines
         (doc_count 1 after reopen; Xapian unchanged). With the guard removed 3 checks fail.
      3. CR3: an `index_fenced` error marks the Core; `acquire` and `reopen()` replace the fenced (or empty) index in
         place with one opened from the last commit, so every live handle sees the fresh one; pending writes are
         dropped (the WAL replays them on a node). `fenced_recovers` (new writable handle while a reader lives, and
         `reopen()`). With recovery disabled both fail.
      4. CR4: a replayed line whose key lives at another id is skipped; the erase of a document that is gone stays
         `doc_not_found` in the engine and `DatabaseWAL::execute_line` treats it as done (both engines; covered by the
         engine test through a helper with the same rule, not by a node run, which was not built). A stamped line is
         applied only when it is exactly stored + 1 (1 for an absent document), skipped when `stored >= stamp`,
         `errc::corrupt` otherwise. `replay_idempotent`: the five double-replay scripts and the gap. Mutants: moved-key
         skip off (1 check), gap rule off (4 checks).
      5. CR5: `index->refresh()` runs before every `commit()`. Not observable through the API, so no failing test.
      6. CR6: `src/engine/strata/keys.h` holds the lead byte, `engine_key()` and `key_prefix()`.
      7. CR7: a failed commit in `~Core` is logged (`L_ERR`).
      8. The document codec's format byte is 2; a format 1 line raises `errc::syntax` naming the version stamp
         (`old_format`; reverting the byte fails the codec, replay and old-format tests).
      9. NF1 (re-check 1): `Core` carries an epoch that every `reopen_index()` bumps; each `Handle` records the epoch it
         has seen (its own replacement counts), and `Core::check` raises `errc::database` for a writable handle that is
         behind. `reopen()` on a writable handle whose index was fenced or replaced replaces a fenced index and raises
         `errc::reopen` (the node's retry then closes the shard and reopens it writable, which replays the log). Tests:
         `fenced_writer_notified` (the re-checker's `probe_fence3` as a test), `fenced_recovers(true)` updated, and
         `tests/test_wal_replay.cc` (the re-checker's `probe_wal` through `DatabaseWAL::execute_line`, both engines).
      Reviewer probe `probe_replay` still reports its erase scripts as errors because it replays through the engine
      alone; with the WAL's rule they converge (`replay_idempotent`). `probe_errors` ends at its own unguarded
      `find_key` after the failed cancel, which now raises `database` instead of crashing.

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

7. Replayed erases are not stamped; the WAL treats `doc_not_found` of a replayed erase as done (CR4).
8. CR8 to CR10 of the review are open: the `apply` internal-error path after the index applied, erase by docid after a
   write forcing a flush, and strata shards having no remote or replica story (architect).

## Blocked on

Nothing.
