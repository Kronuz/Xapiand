# Code review: phase0.5/seam-write (Xapiand:fba3e3a53 on 8170a3ef6)

Reviewer: code-reviewer, independent. Worktree `~/code/Kronuz/Xapiand-wt/review-seam-write` (detached at the
branch). Probes, scripts and logs are in its `.scratch/` (listed at the end); the build dir is deleted.

## Verdict: APPROVE WITH CONDITIONS

The branch does what it says and I found nothing in it that blocks landing. The document is built natively in
the parallel stage, the shard writes that object, the log serialises it, and the bytes that reach the log, the
search results and the `:info` are identical to the parent. The one durability finding (CR1) is a WAL replay
defect that master `cca0011bb` already has and the branch inherits unchanged; it needs an owner before gate 0.5
closes, but it is not caused by this diff and does not gate this merge.

Conditions:

1. CR1 is recorded as a defect against master (repro: `.scratch/replay_schema.sh`, `.scratch/xrep.sh`) and
   handed to the architect or lead for an owner before the gate 0.5 verdict. Durability, so not left as a
   non-blocking note.
2. `docs/status/seam-write.md` says "WAL recovery check" without saying that the harness check counts documents
   only. Reword it so nobody reads it as proof that recovery restores the schema (CR1).

Blocking items for this merge: none.

## What I checked, and the result

| Ask | Result | Evidence |
|---|---|---|
| 1. Seam holds | Holds. `harness/seam_check.sh`: files=7 lines=158, allowlist untouched (`git diff HEAD~1 -- harness` empty). `engine/document.h` includes no Xapian; `Native` is a plain virtual interface over `string_view`/`string`; `xapian_document()` is only used under `src/engine/xapian/` and in tests. | seam_check run; grep of `xapian_document`/`to_xapian`/`from_xapian` outside `src/engine/xapian` shows only `src/database/shard.cc:38,1367-1381` (already allowlisted). |
| 2. WAL format | Unchanged. Serial 150-document index, `kill -9`: every shard `wal.0` is byte-identical between `cca0011bb` and this branch's ASan build except 20 header bytes at offsets 25..56 (the volume UUID). The `5cce4860d` WAL is identical to `cca0011bb`'s the same way. Cross replay works both ways (cca writes, branch replays; branch writes, cca replays) with identical results to master replaying master. | `walrun.sh`, `ops.sh`, cmp output in this session; 20000 random op sequences against a model of the old conversion, 0 byte mismatches (`probe_equiv`). |
| 3. Thread safety | No new shared state. Reasoning below; ASan+UBSan clean on bulk; TSan not run (see Not run). | below |
| 4. Every write path | Index, update, patch, delete, numeric-id and version-conflict paths exercised; replay and remote/replication apply still go through the `Xapian::Document` shims and `from_xapian` (a copy), as designed. | `ops.sh`; `src/database/shard.cc:1366-1382`; `src/database/wal.cc:288,558`. |
| 5. Identity | SEARCH dump (547820 bytes, md5 `fc56819c...`) and `:info` (md5 `8440196...`) equal on my ASan build and on `xapiand-cca`. Docs E2E: 576 of 576 assertions on the ASan+UBSan build, newman 6.2.3, zero sanitizer reports. 18 of 18 unit tests pass under ASan+UBSan. | `dump.sh`; `e2e/asan.json` through `harness/e2e_assert.py`; ctest. |

## Findings

### CR1 (durability, inherited from master; condition 1): WAL replay after `kill -9` leaves the schema ineffective until the next write

Not introduced here: `xapiand-cca` (`cca0011bb`, master) and this branch behave identically; `xapiand-pre`
(`5cce4860d`, before the seam) does not have it.

Repro (`.scratch/xrep.sh`, `.scratch/replay_schema.sh`): `RESTORE` 1000 bank documents into a fresh `--solo`
node without `?commit`, `kill -9`, restart on the same data dir (log shows "Read and execute operations WAL
file ('.xapiand/indexes/.__N/')" five times).

```
replayer pre   : _avg(age) = 30.042   (correct)
replayer cca   : _avg(age) = 0.0      (all numeric aggregations and `_sort=balance` ignore the values)
replayer branch: _avg(age) = 0.0
```

The same WAL directory copied and replayed by `pre` gives 30.042, so the log is right and the replay is wrong.
In the failing case `_min`/`_max` return the float limits and sort by `balance` returns documents in id order.
After one further `PUT` into the index the result is correct again (30.0619...) and stays correct after a
clean restart, so the documents and their values are intact on disk and the loss is in what the replay of the
index-registry WAL (`.xapiand/indexes`, where the schema lives) leaves in effect. Candidate cause: the replay
path through `Document::unserialise` + `Shard::replace_document(engine::Document&&)` (`src/database/wal.cc:558`,
`src/database/shard.cc:1386`) differs from the pre-seam path, which handed the `Xapian::Document` straight to
the database; the branch does not change that logic. I did not isolate it further.

Why this matters for the branch: `docs/status/seam-write.md` lists a WAL recovery check as evidence. The
harness check (`harness/wal_recovery_check.sh`) counts documents and passes; it cannot see this.

Consequence: after a crash, numeric sort, range and aggregation answers are wrong until something writes to the
index. Needs an owner (architect for durability).

Probes: `.scratch/xrep.sh`, `.scratch/diag3.sh`, `.scratch/diag5.sh`, `.scratch/replay_schema.sh`.

### CR2 (non-blocking, seam contract): `Document::Native` calls do not translate Xapian exceptions

`XapianDocument::add_term/add_boolean_term/add_posting` (`src/engine/xapian/convert.cc:53-70`) forward to
Xapian, which throws `Xapian::InvalidArgumentError` for an empty term. Probe: `add_term("")`,
`add_boolean_term("")`, `add_posting("")` all throw the raw `Xapian::InvalidArgumentError` ("Empty termnames are
invalid") from the Schema stage. Before, the same input surfaced as an `engine::Error` (kind invalid) from the
shard's `rethrow_translated` at write time. I could not reach it over HTTP: 22 empty and odd inputs (empty
keyword/term/text, 300-byte keyword, whitespace, empty id, zero weight and position) return byte-identical
responses on master and this build (`.scratch/empties.sh`, `e-cca.out` equals `e-asan.out`), because every
field term carries a prefix and `Schema::index_term` (`src/database/schema.cc:5290-5310`) guards length. So
this is a contract gap, not a live bug. A strata `Native` will throw its own errors; the seam should say that
`Native` calls throw `engine::Error`, and the Xapian one should use `classify_current` like the shard does.
For the architect.

### CR3 (non-blocking, design): the engine is selected at link time

`engine::Document::Document()` and `Document::unserialise` are defined by the engine's translation unit
(`src/engine/xapian/convert.cc:369,374`) and `xapian_document()` is a `static_cast` of the native pointer
(`convert.cc:109-112`). One binary can hold one engine, and passing a document from another engine is undefined.
This is what the mechanism note says (invariant 4), and it is fine for the migration plan if the strata engine
replaces Xapian per build. If the plan ever needs both engines in one process (a migration tool reading Xapian
and writing strata), `Document` needs a factory or the engine passed in. Also `Native::serialise()` bakes the
Xapian stored form into the seam as the WAL payload. For the architect, no code change.

### CR4 (non-blocking, perf): a default `Document` now allocates

`Document()` makes a `XapianDocument` (a heap object plus a `Xapian::Document` with its internal) even when it
is a placeholder. Places that make one and immediately overwrite or ignore it: the default-constructed tuple at
`src/database/handler.cc:2107` (once per dequeued item, and once per 100 ms idle wake), `handler.cc:328` (the
`prepared` tuple in `prepare()`, assigned in the retry loop), and the sentinels at `handler.cc:2013,2241,2282`.
That is up to two wasted allocations per document in the serial-ish indexer loop, plausibly part of the
remaining 3 percent. A moved-from `Document` also has a null `native_` (every method dereferences it), where a
moved-from document used to be empty and safe; I found no use after move (`handler.cc:419-422`,
the move into `write_replace_document` at `shard.cc:1336` is the last use). Follow-up: hold the document in a
`std::optional` in those tuples or make the default lazy.

### CR5 (non-blocking, perf): replay and remote apply still copy

`Document::unserialise` and the `Xapian::Document&&` shims (`shard.cc:1366-1382`) rebuild the document through
`from_xapian` although the source is an rvalue and `XapianDocument(Xapian::Document)` exists
(`convert.h`). Wrapping would save one walk of the terms per replayed or remotely applied document. Not on the
bulk path; leave for the cleanup pass.

### CR6 (non-blocking, tests): the differential test is now self-referential

`test_matches_xapian` (`tests/test_engine_document.cc:107`) compares an `engine::Document` with a
`Xapian::Document` built by the same calls; since the first is now a Xapian document, a regression in how the
callers (Schema, GenerateTerms) call it cannot show up there. What still protects that is the SEARCH-dump and
`:info` identity and the E2E. I filled the gap once with `.scratch/probe_equiv.cc` (the old map model with the
verbatim `to_xapian` of `cca0011bb` against the new document: 20000 random sequences including empty values,
position 0, wdf 0 and wdf beyond 32 bits, 0 mismatches, and the `unserialise` round trip reserialises to the
same bytes). The new check "the document left by a write is the stored one" (`tests/test_engine_shard.cc`)
does pin the shard-writes-this-object claim. No action.

## Thread safety (item 3) in file:line terms

1. The document is created per document in the preparer thread: `Schema::index` builds a local
   `engine::Document doc;` (`src/database/schema.cc:2975`); nothing in the build reads a shared document.
   Shared analysis state (`getStemmer`, `getStopWords`, `getStopper`, `makeStem` in
   `src/multivalue/generate_terms.cc:506,542`, `src/engine/xapian/text.cc:82,86`) is unchanged by the diff.
   The word-break path keeps its `Xapian::TermGenerator` and `Xapian::Document` local to the call
   (`text.cc:69-71`).
2. The hand-off is by move with unique ownership: preparer to indexer through the queue
   (`handler.cc:1995-2000`, dequeue `handler.cc:2107`), indexer to the shard write (`handler.cc:2127`), and for
   the log into `DatabaseWALWriterTask::doc` (`shard.cc:1336,1446,1557`, `wal.cc:1194`, serialised on the WAL thread at
   `wal.cc:1057`). The shard thread does not touch the document after the move. Commit/delete tasks hold an
   empty `std::optional` (`wal.h:271`); `doc->` is only dereferenced in the replace task.
3. The one assumption I could not verify without reading the Xapian fork: that `WritableDatabase::replace_document`
   does not keep a reference to the `Xapian::Document` internal after returning (otherwise the WAL thread
   would race on its reference count). The developer's new test shows the object left after the write
   serialises equal to the stored document, and ASan/UBSan show no error, but that is not a race check. A TSan
   run of the bulk path is the cheap proof; the macOS toolchain notes say Apple clang's TSan crashes at thread
   creation, so it needs Homebrew LLVM or the Linux CI.

## Other results

1. Unit tests: ctest 18 of 18 under ASan+UBSan (`xapiand_test_engine_document`, `_shard`, `_search`, etc.).
2. Bulk (parallel DocPreparer and DocIndexer) under ASan+UBSan: 20 repetitions of the bank with unique ids
   (9180 documents), count and `_sum(balance)` equal to master (9180; 23286266.0); no sanitizer output in the
   node log. 6 repetitions, `kill -9`, replay with branch and with master: same counts.
3. Mixed ops (150 PUTs, 5 replaces, 3 patches, 3 deletes, 3 numeric-id POSTs, a 409 version conflict and a 204
   versioned write): the search hash before the crash is equal on master and branch; after replay the hash is
   equal across all four writer/replayer pairs, and equal to master replaying master (it differs from the
   pre-crash hash on master too; that is CR1).
4. Reachable error responses (empty values, too-long keyword, invalid uuid id): identical on master and branch.
5. Scope: the diff stays inside the design (document handle, shard, log task, word-break path, tests, two docs).
   No allowlist growth, no format change, no new dependency. The docs mechanism note is accurate against the code.

## Not run

1. TSan (Apple clang unusable for it; no Homebrew LLVM build made). Race freedom rests on the reasoning above
   and one assumption about the Xapian fork. Suggest the bulk RESTORE plus async WAL under TSan on Linux CI.
2. Release+LTO build and any benchmark (not allowed); the 4635 vs 3704 docs/s numbers are the developer's and
   not rechecked. Only the correctness of the written bytes was checked.
3. Linux, FreeBSD, arm64.
4. Cluster paths: remote and replication apply with a live replica (the Xapian shims in
   `remote_protocol_views.cc`, which I did not read per the instructions).
5. A mutant of my own (the developer reports one failing 536 of 645 checks; I relied on that).
6. Root cause of CR1.

Baselines used: the developer's Release binaries `xapiand-cca` (reports git:cca0011bb) and `xapiand-pre`
(5cce4860d per the ledger) in `~/code/Kronuz/Xapiand/.scratch/seam-write/`, as black-box comparators only.
`xapiand-v4b` (the developer's branch build) was used once to confirm CR1 on a Release branch binary; my own
ASan build of `fba3e3a53` reproduces CR1 independently (`.scratch/diag.sh` with REPS=1: DIFFERS; log `dg8.log`). The `newman` on PATH is 4.5.0
and fails under node 26 (every request reports HTTP None); I used newman 6.2.3 from
`~/.scratch/newman6/node_modules/.bin`.

## Probes and artifacts (all in `~/code/Kronuz/Xapiand-wt/review-seam-write/.scratch/`)

1. `probe_equiv.cc` (binary deleted; rebuild with `build-asan.sh` then `buildprobe.sh`): old-model vs new Document, empty-term exceptions. `buildprobe.sh` builds
   any probe with the test target's flags.
2. `probe_unser.cc`: binary values on large slot numbers through `Document::unserialise` (round trip equal).
3. `srv.sh`, `walrun.sh`: start a solo node; serial 150-document index then `kill -9` for the WAL byte compare.
4. `ops.sh`: mixed write ops, dump hash, `kill -9`, replay by a second binary.
5. `bulk.sh`, `flaky.sh`, `matrix.sh`: parallel bulk path, `kill -9`, replay, aggregation fingerprint.
6. `xrep.sh`, `diag.sh`..`diag5.sh`, `replay_schema.sh`: CR1 isolation (writer/replayer matrix on copies).
7. `empties.sh`, `e-cca.out`, `e-asan.out`: empty and odd term inputs on both builds.
8. `dump.sh`: SEARCH dump and `:info` hashes. `e2e/` (the 61 MB newman 6 report was deleted after `e2e_assert.py` printed 576 of 576 green).
9. `build-asan.sh`, `ubsan-ignore.txt`: how the sanitizer build was made (`-DASAN=ON -DUBSAN=ON -DBUILD_TESTS=ON`).
