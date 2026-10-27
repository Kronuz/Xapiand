# Xapiand code review: engine search, step X4b (`engine::search`, `PreparedQuery`, `db()` removed, remote server on `native<>`)

- Date: 2026-10-27.
- Branch: `phase0.5/engine-search-b`, head Xapiand:5e0206848 (five commits over local master Xapiand:593b78293: Xapiand:d3f914396 engine::search, Xapiand:d160f6ba8 handler and consumers, Xapiand:d4307953c `db()` removed, Xapiand:9a76de01e ledger, Xapiand:5e0206848 remote server edit). Local only, nothing pushed.
- Tier: shared API (`engine::search` is the seam strata implements) plus a larger change that touches the remote protocol (14 files, +1,182/-307 in the diff I could read).
- Reviewed against: `docs/status/engine-search.md` (X4b criteria 1 to 5, proof, "For the lead"), `docs/reviews/engine-search-x4a-code.md` (house rules), the X4a notes N1 to N7. `docs/reviews/calibration.md` does not exist in this repo (same as X4a).
- Reviewer: code reviewer, fresh context. Never opened `src/xapian/`, `src/sortable_serialise.*` or `src/server/remote_protocol_views.cc`; every diff and grep excluded them. Contact with the GPL file: `harness/seam_check.sh` runs `grep -c` on it; I read `.scratch/remote_edits.sh`; and two review mutants rewrote the one `get_document(` call with a regex (see the ruling below), never reading the file. `git diff --stat` shows its size only.

## Verdict: APPROVE WITH CONDITIONS

Nothing blocks landing. The final head (5e0206848, with the remote edit applied) builds, passes 17 of 17 unit tests under ASan and UBSan, answers all four corpora byte-identical to the baseline binary, and the 2-node cluster run (sections 3d and 3e: search over the remote protocol) is GREEN with bodies identical to the baseline. The developer's evidence was at d160f6ba8; it holds at the head. The retry question has a ruling: the remote `get_document` can now return `DatabaseModifiedError` to the client, the client already retries it, and no condition is needed (ruling below). One condition: the unit suite does not lock how the "relevant set" for `nearest`/`fuzzy` is built, and `search.h` does not say it (C1).

## Ruling: the remote server's `get_document` lost its reopen-and-retry loop

Question: can a remote GET or remote search now hit `DatabaseModifiedError` under concurrent writes where it could not before?

Answer: it can reach the client now, and the client absorbs it. No retry at the seam is required.

1. How it reaches the client. The old `Shard::get_document` retried on the server (up to `DB_RETRIES` = 10 with `reopen()`), so a stale server reader never answered with the error. Now the server's `get_document` throws it, and the remote protocol carries it to the client as a `REMOTE:`-prefixed `Xapian::DatabaseModifiedError`. Proof by injection: mutant `.scratch/mutants/probe_srvthrow.patch` (a regex rewrite of the `get_document(` call to a template `probe_get(db, ...)` in `.scratch/mutants/probe_throw.h`, plus a stderr print in each client ladder) throws the error on 30% of the server's calls. In 40 s with 2 writers and 6 readers, 18,000 server calls threw 5,392 times; the client's `Shard::get_view` ladder (`shard.cc:1728`, 10 retries with `reopen()`) logged 9,737 retries and every GET answered 200 (43,566 of 43,566; `.scratch/cm-throw30.out`). At 0% the same run: 44,126 of 44,126.
2. Every client path to a remote document goes through a ladder that handles the error. `git grep` outside the GPL file: `get_view` has seven callers in `handler.cc` (all through `Shard::get_view`, whose ladder catches `DatabaseModifiedError`), and the two `scan` callers (the dump forms) have their own ladder that resumes at `shard_did`. No caller reads a remote document without one.
3. Real concurrent writes did not trigger it. Four stress runs with the unmodified head and a client-ladder print (`.scratch/probes/cluster_modified.sh`, `modified_load.py`; 2 nodes, 4 to 8 writers committing every PUT on both nodes, 6 to 8 readers on both nodes; 60 to 90 s each): 69,384 GETs of 20 hot documents against 51,455 commits, zero `get_view` modified errors and zero failed reads; 35,193 GETs and 15,382 SEARCHes with 26,483 commits, zero failed reads. Old binary (X4a head, which still has the loop): the same load, zero failed reads in three runs. So the exposure is theoretical for GET today and absorbed if it occurs.
4. Search is the real exposure and it is not new. The same stress with the print in `DatabaseHandler::match`'s ladder shows 2,408 `REMOTE:The revision being read has been discarded` errors in 60 s (`.scratch/cm-probe.out`, handler.cc:1252 of the mutant), all absorbed by the existing 10-retry ladder (15,547 of 15,547 SEARCH answered 200). The remote server's match path never had a server-side retry; X4b changes none of it.
5. What is lost. A budget: before, a stale reader cost up to 10 server retries inside each of 11 client attempts; now only the client's 11. A removed test hook: `RANDOM_ERRORS_DB_THROW` no longer fires for the remote `get_document`; it only matters in a random-errors build.

Optional (not a condition): if the owner of the remote file wants the server to absorb it as before, the exact replacement is, in `engine/xapian/shard.h`, a helper

```cpp
// Reads a document of the shard's database, reopening and retrying when the database is modified.
Xapian::Document get_document(::Shard& shard, Xapian::docid did, unsigned flags = 0);   // loop: DB_RETRIES, catch DatabaseModifiedError -> shard.reopen(), re-fetch native database()
```

and the counting script `perl -pi -e 's/\blk_shard->engine\(\)\.native<engine::xapian::Shard>\(\)\.database\(\)->get_document\(/engine::xapian::get_document(*lk_shard, /g'` with the expected count 1 before and 0 after, the `database()` native count going from 23 to 22. I would not do it: it puts the pool's `Shard` type back into `engine/xapian`, which the seam is trying to leave.

## What I checked and found sound

1. Build and unit tests: `.scratch/build-x4b.log`, ASan and UBSan, RelWithDebInfo, `-DBUILD_TESTS=ON -DLTO=OFF`, x86_64 macOS, `quiet light`, -j4, no compile errors. `ctest -j4`: 17 of 17 (`.scratch/ctest-x4b.log`), `test_engine_search` 3,497 checks.
2. Behaviour drift, four corpora against the baseline binary (`engine-search/.scratch/xapiand-base`), `cmp` identical, rerun by me on the head: `qdiff2.py` (the 2,007-answer corpus; the developer's `qdiff.py` is the same file), `qnf.py` 104, `qread.py` 911, `qsim.py` 265 (`.scratch/corpora_x4b.sh`, `corpora_x4b_2007.sh`, outputs `.scratch/e2e/x4b-*-{new,base}.json`). No AddressSanitizer report in any node log; no `blocks on database work` line; UBSan lines are the lz4, rapidjson and `engine/xapian/shard.cc:72/297` ones that the baseline prints (X4a N5).
3. Cluster: `cluster_range_x4b.sh 2` (the developer's script with 3d and 3e) GREEN, 30 of 30 (`.scratch/cluster-x4b.out`). The 12 normalised saved bodies (dump, info, check, plain, window, nearest, fuzzy, both, collapse) are `cmp` identical to the developer's baseline set (`engine-search/.scratch/cl3-base`). No ASan report, no `blocks on database work`, no modified-error line in the node logs.
4. Search paths over corrupt and missing shards: `.scratch/probes/p4_search_paths.py` (a 3-shard index with one postlist overwritten, a 1-shard index, `ix,iy`, `ix,nosuch`, `nosuch`; stop-word text, normal, `nearest`, `fuzzy`, match-all, COUNT: 42 answers): new and baseline `cmp` identical (`DatabaseCorruptError: dir_end invalid in block 2` as the 500 body, a missing index answers an empty 200).
5. The handler's retry ladder (`handler.cc:1235-1275`) is the old ladder: `engines` is rebuilt from `shard->engine()` at every attempt (so no `engine::Shard*` survives the `reopen()` of the catch), `PreparedQuery` is created before the checkout and outlives the loop, the `Scope` (database copy, key-maker and spy adapters) is local to `run()` and dies before `search` returns, and the `mset` in `run` is destroyed before the scope that its iterators depend on. Exceptions from `search` come through `rethrow_translated`; the wildcard and parse errors still reach the `QueryParserError` ladders (corpus).
6. `nearest`/`fuzzy`: the relevant set is now taken from the same combined database as the main search, with collapse, cutoffs, docid order set as the old `get_rset`'s call to `match` did (ascending, no collapse, no cutoff, no sorter, no spy), and the elite sets are computed on the enquire configured with the sorter and spy as before. Equal to the old behaviour on the corpus (`qsim.py`, which has different settings for the two). One behavioural change by construction: the old code took the relevant set through a separate checkout and ladder, the new one uses the same snapshot, so a commit between the two phases can no longer make the sets disagree. An improvement, not drift.
7. Weight, rank and percent are filled for every hit (`search.cc` copy loop). No consumer used them lazily in a way that changes; the only HTTP use is the `comments` flag, and the corpora with `comments` are identical. Cost is three iterator calls per returned hit; not measured (no benchmarks).
8. The consumers (`search_view`, `count_view`, `expand_paths`, `manager.cc` x3): totals come from `estimated`, counts from `hits.size()`, the loop body unchanged. `Hit::percent` is `int` as Xapian's.
9. `seam_check.sh`: exit 0 on the head (files=38 lines=622, as the ledger says). The ceilings were lowered by `--update` to exactly the actual counts for `handler.cc` 168, `handler.h` 24, `shard.cc` 43, `shard.h` 11, `search_views.cc` 14. The remote file's `engine/xapian/shard.h` include and the 23 `engine::xapian::Shard` mentions do not move the counter (it counts `Xapian::` and `xapian.h`), which matches the design: the remote server keeps its Xapian ceiling and the native handle is the seam's escape hatch (see N3).
10. `Shard::db()` and `Shard::get_document(docid, flags)`: no caller left (`git grep` outside the GPL file and `src/xapian`; the remote file is the only one that built against them and builds with the edit). `get_edecider` and `get_rset` are gone.

## The two surviving mutants

1. I2 (the lower-case type prefix). Confirmed, and it is a pre-existing bug, not an X4b one. `get_similar` is `get_edecider`'s body moved verbatim (`git show master:src/database/handler.cc` lines 748 to 765 against `handler.cc:695-721` differ only in the return type). In the saved corpus answers (`e2e/x4b-qsim-new.json`, identical to `-base`) every `nearest.type=` request (`text`, `keyword`, `integer`, text plus integer, with and without a field) answers `count` 0 against 35 for plain `nearest` and 25 for `nearest.field=title`; every `fuzzy.type=` request answers the same count as `fuzzy` without any expansion (19, 4). So the type filter is a filter that matches nothing: `starts_with(type_char)` or `starts_with(tolower(type_char))` only matches terms with no field prefix, and field terms begin with the field's prefix. For the architect: either the feature is meant to filter on the type character inside the prefix (then the matching is wrong) or it should be dropped; the decider's prefix list is now data in `engine::Similar::prefixes`, so strata inherits the question. Not a blocker: unchanged behaviour, nothing in the docs corpus relies on it.
2. I12 (the empty-query early return in `match`). Equivalent in output, by construction: `engine::search` returns an empty result for an empty `PreparedQuery` (`search.cc`, the `shards.empty() || prepared.empty()` guard), so the handler's guard only avoids the shard checkout. I found which inputs reach it, with a one-line stderr probe (`.scratch/mutants/xapiand-probe-emptyq`, `.scratch/probes/p5_emptyq.py`): `{"_query":{}}`, `_and: []`, `_or: []` and `_match_none` reach it; a stop-word text query (`"the"`, `"of the and"`, even with `_language: en` and `_stop_strategy: stop_all`) does not, because the text analysis yields a non-empty Xapian query, so the developer's corpus could not reach it. The guard cannot be shown by output on a corrupt shard either, since the schema read fails first (the same 500 in the baseline). It would be killed only by a test that proves no checkout happens (the pool's checkout count). Non-blocking; if the guard is kept as a cost guard, say so in the comment.

## Conditions

### C1. The unit suite does not lock how the relevant set is built, and `search.h` does not say it

I re-ran the unit-level mutants in `.scratch/mutants/mut_search.py` against `test_engine_search` (each built and run, source restored after): M1 (percent 0) caught (815 of 3,497 fail), M6 (an empty prefix list rejects everything) caught, M7 (the single shard added twice) caught. Four survive the unit suite: M2 (`nearest` uses `fuzzy`'s `n_term` when both are set), M3 (the relevant set uses descending docid order), M4 (it keeps the request's collapse), M5 (it keeps the request's cutoffs). The corpora catch M2, M3 and M4 (`mut_search_int.py`: qsim, qnf), so the shipping code is protected today; M5 survives everything, but the handler never sets a nonzero cutoff (`handler.cc:1128-1129` are constants), so it is not reachable from a product request. The ledger's "20 of 21 caught" is for the mutants it chose; these four are the contract that the strata implementation of `engine::search` must also follow, and the header gives it no words.

Fix: (1) in `search.h`, on `Similar` or `SearchRequest::nearest`/`fuzzy`: "The top hits are taken by relevance over all documents: the window, collapse, cutoffs, sorter, spy and docid order of the request do not apply to them"; (2) in `test_engine_search`, a case with `nearest` and `fuzzy` set together with different `n_term`, and a case with a collapse key, a percent cutoff and descending order set together with `nearest`, each compared with the hand-written Xapian search as the other cases are (kills M2 to M5). Cheap; no behaviour change.

## Non-blocking notes

### N1. The ledger's seam claim and its evidence head

"`MSet`, `Enquire`, `RSet`, `ESet` outside `src/engine/xapian`: 0" is not exact: `remote_protocol_views.h:93,131,149` names them (a `std::unique_ptr<Xapian::Enquire>` member, and message comments); that is the remote server, which keeps its ceiling. Reword to "outside the remote and replication files". The proof section also says the corpora, E2E and cluster ran at d160f6ba8; I reran the corpora and the cluster at the head, and the docs E2E remains the developer's d160f6ba8 run (see Not run).

### N2. Writes under concurrent load sometimes fail with `Cannot update schema while preparing: Too many retries`

Seen as 1 to 3 PUT 500s in about 27,000 commits in several stress runs with the head. The X4a binary shows 1 in one of three runs under the same load, so it is not an X4b effect (the write path is untouched). Worth a follow-up for whoever owns schema updates; I did not chase it.

### N3. `native<engine::xapian::Shard>().database()` now appears 23 times in a file the seam counter does not watch

`seam_check.sh` counts `Xapian::` and `xapian.h`, so the 23 `engine::xapian::Shard` mentions are invisible to it, though they are the escape hatch that keeps the remote server on Xapian. The end state (X4c) allowlists the remote and replication files, so this is consistent, but a second counter on `native<` (growth fails) would stop the hatch from being used elsewhere. Carries over X4a N3/N4 (hold nothing from `native<T>()` across a reopen; the remote server's per-message `engine()` call keeps this true).

### N4. Pre-existing: a skipped missing shard shifts the docid-to-shard mapping

`match` skips a shard whose index does not exist (`engines` shorter than `shards`), so the combined docids interleave over fewer shards than `get_document(did)` assumes. The same code did this before (`valid` count), the corpora with a missing index in a multi-index search are identical, and the missing index there has a single shard. Only mentioned because `engine::search` documents "interleaved the way the shards number them" and a caller passing fewer shards than endpoints gets this.

### N5. The `Similar` prefixes are copied twice per request

`get_similar` builds a `std::vector<std::string>` and the request copies it; trivial next to the search. Not measured.

## Reproduced

1. Build ASan and UBSan, `ctest`: 17 of 17.
2. Four corpora against the baseline: identical (qdiff2 2,007, qnf 104, qread 911, qsim 265).
3. Cluster `cluster_range_x4b.sh 2`: GREEN, 30 of 30, bodies identical to the baseline's.
4. Corrupt-shard and missing-index search probe `p4_search_paths.py`: identical to the baseline.
5. Remote retry: stress runs on the head (reads never failed), on the X4a binary (reads never failed), and the injection mutant (absorbed).
6. Mutants: M1 to M7 on the adapter; M2 to M5 on the integration corpora.
7. `seam_check.sh`: exit 0.

## Probes and files kept (all in `.scratch/` of the review worktree `Xapiand-wt/review-eq`)

`probes/cluster_modified.sh`, `probes/modified_load.py` (the remote read-while-write stress; env `GET_FRAC`, `WRITERS`, `READERS`, `DURATION`, `NDOCS`, `BIN`), `probes/p4_search_paths.py`, `probes/p5_emptyq.py`, `mutants/mut_search.py`, `mutants/mut_search_int.py`, `mutants/probe_modified.patch`, `mutants/probe_srvthrow.patch`, `mutants/probe_throw.h`, `mutants/probe_emptyq.patch`, the three probe binaries `mutants/xapiand-probe-{modified,srvthrow,emptyq}`, `corpora_x4b.sh`, `corpora_x4b_2007.sh`, `cluster_range_x4b.sh` (copy of the developer's), `qsim.py` (copy), `qdiff2.py`, `qnf.py`, `qread.py`; outputs `e2e/x4b-*.json`, `e2e/cm-*.json`, `e2e/p4-*.json`, `cm-*.out`, `cluster-x4b.out`, `ctest-x4b.log`, `build-x4b.log`. The source tree is clean (`git checkout -- src` after each mutant); the fresh `build-asan` is deleted.

## Not run

- Docs E2E (339 requests, 576 assertions) on the head: the corpora, the cluster run and the stress cover the changed paths; the developer's run was at d160f6ba8.
- TSan (`engine::search` runs inline on the calling thread, the only shared state is two atomics), GCC, Linux, FreeBSD, arm64, LeakSanitizer (not available on this host; the live counters stand in), a 3-node cluster, failover, benchmarks, libFuzzer (no decoder was added).
- The remote-protocol file itself was judged only by building and running, as agreed; the random-errors build (`RANDOM_ERRORS_DB_THROW`) was not exercised.
