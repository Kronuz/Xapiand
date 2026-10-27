# Xapiand code review: engine search, step X4a (the engine read slice)

- Date: 2026-10-26.
- Branch: `phase0.5/engine-search`, head Xapiand:0f4dd12c8 (two commits over local master Xapiand:1ab0ce077: Xapiand:3e3ff8f10 code, Xapiand:0f4dd12c8 ledger). Local only, nothing pushed.
- Tier: shared API (`engine::Shard`'s read methods and `native<T>()` are the seam strata implements) and a larger change (20 files, +967/-178).
- Reviewed against: `docs/status/engine-search.md` (X4a criteria 1 to 6, proof, remote edit list), `docs/reviews/engine-query-code.md` and `engine-query-2-code.md` (house rules), `strata/docs/design/xapiand-on-strata.md` (X4 row, D11, D13). `docs/reviews/calibration.md` does not exist in this repo.
- Reviewer: code reviewer, fresh context. Never opened `src/xapian/`, `src/sortable_serialise.*` or `src/server/remote_protocol_views.cc`; every diff and grep excluded them. The only contact with the GPL file is `harness/seam_check.sh` running `grep -c` on it, as the developer's script does.

## Verdict: APPROVE WITH CONDITIONS

Nothing blocks landing. Every migrated read returns what the Xapian call returned, including the error paths: 911 read answers, the corrupt-shard and CLI-dump probes, the cluster run and 16 of 16 unit tests under ASan and UBSan are all identical or green, and no sanitizer or blocking-guard report comes from the changed code. One done criterion (the live counter for the new view objects) is ticked without evidence (C1); the rest are non-blocking notes.

## What I checked and found sound

1. Behaviour drift, read corpus. `.scratch/qread.py` (the developer's, 911 answers: DUMP, INFO, CHECK of 1, 3 and 5-shard indexes, INFO of documents 0 to 45 raw and human, GET and HEAD of every id, metadata keys, missing indexes) run against the developer's baseline binary (`engine-search/.scratch/xapiand-base`) and my ASan build of 0f4dd12c8: `cmp` identical (`.scratch/e2e/qr-new.json`, `qr-base.json`), and identical to the developer's own `qr-base.json`. Both logs print the same lz4, rapidjson and `engine/xapian/shard.cc:72/297` UBSan lines (pre-existing, N5); 0 AddressSanitizer reports; no `blocks on database work` line from `engine::check_may_block` in any log (so none of the new `sync_wait` sites runs on a reactor thread).
2. Error paths of `check`, `info` and `dump`, which the corpus reaches only for a missing index. Probe `.scratch/probes/p3_paths.py` (boots a 3-shard and a 1-shard index, then overwrites the header block of one shard's `postlist.glass` and the middle of another's `docdata.glass`, reboots and asks CHECK, INFO, DUMP, GET and INFO of a document; also the CLI `--dump` of both indexes before the damage and of the damaged one): new and baseline outputs are identical apart from the version line, run on both binaries. The new binary reports `"c3/.__2":"DatabaseCorruptError: dir_end invalid in block 2"` as a value in CHECK, and a 500 with the same message for DUMP, INFO and the document INFO, as the baseline. The CLI dump (`dump_documents(int fd)`, the file form that no HTTP test reaches) writes the same 1,041 bytes and the same sha256 for both indexes.
3. `check` returns errors as values and swallows nothing. `handler.cc:289-296` maps `failure` to the body text and `errors` to the count; non-Xapian exceptions still reach `catch (...)` ("Unknown error"). Mutant H1 (`.scratch/build-h1.log`; handler always writes `result.errors`): the read corpus differs in the missing-index CHECK (`nosuchindex/.__N` come back as 0 instead of `DatabaseOpeningError: ...`), and the corrupt-shard probe differs on `c3/.__2`. So a swallowed failure cannot pass both. (The mutant tree was restored with `git checkout`; the review tree is clean.)
4. Dump retry. `scan` sets `shard_did` in the sink, after the document exists, where the old loop set it before `get_document`. Because a Xapian document is lazy, the failure that the retry exists for (the modified-database error) surfaces in `doc.data()`, after the assignment in both forms. Probe `.scratch/probes/probe_dump.cc` (a reader invalidated mid-scan by three writer commits that replace documents 5 to 40, triggered at documents 3, 7 and 20, with the handler's `initial = shard_did` resume): the old Xapian form and the `scan` form deliver the same count (49, 49, 57) with 0 duplicates and 1 retry each. The translated exception (`Reopen`, derived from `Xapian::DatabaseModifiedError`) is still caught by the handler's ladders. One narrow window differs (N1).
5. `Shard::engine()` replacing `db()`. `engine()` is the body of `db()` minus the final `->database()`; `db()` now calls it, so the opened, reopen and closed rule is one piece of code. Every read ladder (`get_docid_term`, `get_view`, `get_metadata`, `get_metadata_keys`) re-fetches `eng = &engine()` after `reopen()`, which replaces the `unique_ptr` (`shard.cc:602`), so none keeps a reference across a reopen. In the replication views, `db = &lk_shard.lock()->engine()` is reassigned before each use (`replication_protocol_views.cc:513-680`); the one long-held pointer (`reply_end_of_changes`, `:827` to `:851`) has the same lifetime as the `Xapian::Database*` it replaces. Concurrency: shards are checked out exclusively, `on_` is the empty scheduler (the `Hop` is inline), and `sync_wait` blocks the caller, so no read overlaps another on one shard. The cluster run (`.scratch/cluster-x4a.out`, `cluster_range_x4a.sh 2`, with the extended section 3d: DUMP of 30 documents, INFO of the index and of a document, CHECK, all from the non-owner node so the remote shards answer) is GREEN, 24 checks, 0 failures.
6. `native<T>()` (`engine/shard.h`). `dynamic_cast<T&>(*this)`: the right T returns the object itself, a wrong polymorphic T throws `std::bad_cast`, a non-polymorphic or unrelated non-class T does not compile. Test `test_native` covers the right type (pointer identity and `database()`) and an unrelated type. It adds no new lifetime hazard beyond `db()`: the returned reference lives as long as the `engine_shard` object, which a reopen that replaces it destroys (N3).
7. `get_docid_term`'s not-found is now `engine::xapian::DocNotFound`, which derives `Xapian::DocNotFoundError`, with the same message, so every catch site still matches; the HTTP 404 body is identical (`c1 get 31` and the corpus' missing documents).
8. DocView additions. `values` and `term_details` read through Xapian iterators; `term_details` still swallows only `InvalidOperationError` around `get_termfreq`, as the endpoint did. A corrupt stored document raises the Xapian exception through the sink's caller (the corrupt-shard probe's 500s above), nothing terminates; the throwing-sink test (`test_scan`) shows a non-Xapian exception passes through `scan` unchanged and leaves the shard usable.
9. `harness/seam_check.sh`. Exits 0 on the tree (38 files, 701 lines, as the ledger says). Growth fails: appending one `Xapian::` line to `pool.cc` gives `grew from 4 to 5 lines`, exit 1; an unlisted file naming Xapian gives `is not in the allowlist`, exit 1; shrinking fails with `run --update`. It is the first step of `ci.yml`'s only job, after checkout, and the file is mode 100755. Gaps are minor (N4).
10. Unit tests: 16 of 16 pass under ASan and UBSan (`.scratch/ctest-x4a.log`); `test_engine_read` 591 checks, each compared with Xapian itself. Spot-check of mutants (the developer's `mutants4.py`, my copy `.scratch/rmutants4.py`, output `.scratch/rmutants4.out`): R5 (term details drop positions) fails 37 checks; R12 (metadata truncated at NUL) fails 1. Both as the ledger says.
11. `Shard::db()` use outside the remote server: two sites, both in `match` (`handler.cc:1300, 1308`), as stated; `Shard::get_document` has no caller outside the GPL file. The ledger's count (30 sites to 2 outside the remote server) holds.

## Conditions

### C1. Criterion 6 is ticked without its live counter, and a leaking mutant survives

The ledger's criterion 6 asks for "an ownership test with a live counter for the new view objects" and the done list says all six are ticked. `tests/test_engine_read.cc` has `test_view_outlives_shard` (a view read after its shard is gone) and no counter; `XapianDocView` is in an anonymous namespace (`convert.cc`) with nothing counting it. LeakSanitizer does not run on this host (`detect_leaks is not supported on this platform`), so nothing else stands in for a counter.

Probe (`.scratch/build-leak.log`): `Shard::scan` with `view.release();` after `sink(*view)` (every document's view leaks): `xapiand_test_engine_read` prints `OK: 591 checks passed`, exit 0. The `unique_ptr` in `scan` and `get` is obviously correct today, so this is low risk, but the strata engine will implement `get` and `scan` against the same contract and the criterion is the one that guards it.

Fix, either: (1) count live views in a test hook (a static atomic in `convert.cc` incremented in the constructor and decremented in the destructor, read through a small function declared in `engine/xapian/convert.h`, the way the X3 steps counted adapters) and assert zero after `get`, `scan`, a throwing sink and `view_outlives_shard`; or (2) reword criterion 6 and the done list to what exists ("a view that outlives its shard") and record the gap for the platform that has LeakSanitizer (CI on Linux).

## Non-blocking notes

### N1. One extra window for a duplicate in the dump retry

If `get_document(K)` itself throws the modified-database error (the document-length lookup inside `get_document`, before the sink runs), `shard_did` is still K-1, the retry resumes at K-1 and dumps it twice; the old loop set `shard_did = K` first and resumed at K. The old loop already duplicated K-1 when the postlist advance threw, so this is the same class in a narrower window and not a regression of the common path (item 4). Cheap fix when someone is in there: resume from the last pushed id plus one in both forms, which also closes the old window. For the file form the duplicate would also be in the sha256.

### N2. `TermDetail::term` is valid only during the sink call

The view points at a local string in `term_details` (`convert.cc`); a sink that stores `detail.term` dangles. `doc_view.h` says nothing. One comment line ("valid during the call; copy to keep") belongs on `TermDetail` and `ScanSink` for strata's implementer.

### N3. Hold nothing from `native<T>()` or `engine()` across a reopen

`reopen()` and `do_close()` replace `engine_shard`; a `Xapian::Database*` from `native<...>().database()` (the remote server's 22 sites) dies with it, exactly as the `db()` pointer did. Not new, but the seam is the place to say it: `native()` and `Shard::engine()` should carry "valid until the next `reopen`, `do_close` or checkin" in their comments.

### N4. `seam_check.sh` counts lines, not references

Two `Xapian::` names on one line, `using namespace Xapian;` and `#include <xapian.h>` (angle brackets) do not move the count, so the ceiling can be passed or held with a reference added. Counting matches (`grep -o | wc -l`) and widening the include pattern would tighten it. Shrinking also turns CI red until someone runs `--update`, so two branches that both remove references will conflict on `seam_allowlist.txt`; the architect may prefer a warning on shrink and a failure only on growth.

### N5. Pre-existing UBSan lines in `engine/xapian/shard.cc:72 and :297`

A downcast to `Xapian::WritableDatabase` of an object that is a plain `Xapian::Database`, and a member call through it (UBSan: `downcast of address ... does not point to an object of type`). Both appear in the baseline log too (X2 code, not this step); worth a follow-up since `native<T>()` gives more callers a handle to the engine object.

### N6. For the architect: `DocView` methods do not translate exceptions

`get`, `scan` and the other task-returning reads translate Xapian exceptions into `Failure<...>` (which derive the Xapian types) through `rethrow_translated`; `DocView::data`, `value`, `values`, `term_details` and `serialise` are plain calls and throw raw Xapian types. Today every caller catches the Xapian types, so nothing differs. When X4c moves the ladders to `engine::Error` kinds, a corrupt stored document read through a `DocView` will not carry a kind, and a strata `DocView` will throw `engine::Error`. The contract should say which, before X4c.

### N7. `scan` makes one view per document

Disclosed in the ledger (the dump made one `Xapian::Document` per document before). Not measured by me (no benchmarks by rule).

## Reproduced

1. Build: ASan and UBSan, RelWithDebInfo, `-DBUILD_TESTS=ON -DLTO=OFF`, x86_64, `.scratch/build-asan` in `Xapiand-wt/review-eq` at 0f4dd12c8, `quiet light`, -j4, no compile errors. Deleted when finished.
2. `ctest`: 16 of 16.
3. Read corpus: 911 answers identical to the baseline binary.
4. Corrupt-shard, CLI-dump, crash-restart probe `p3_paths.py`: identical to the baseline apart from the version line.
5. Cluster: `cluster_range_x4a.sh 2` GREEN.
6. Mutants: R5, R12 (unit), H1 (integration, caught by the corpus and by the probe), the view-leak mutant (survives; C1).
7. `seam_check.sh`: exit 0, growth fails twice, as the ledger says.

## Probes and files kept (all in `.scratch/` of the review worktree)

`probes/probe_dump.cc` (built with the existing `probes/build_probe_sort.sh`; binary `probes/probe_dump`), `probes/p3_paths.py`, outputs `probes/p3-new.json`, `probes/p3-base.json`, `probes/p3-h1.json` and their `.out` and logs, `qread.py` (copy of the developer's), `e2e/qr-new.json`, `e2e/qr-base.json`, `e2e/qr-h1.json`, `rmutants4.py` and `rmutants4.out`, `cluster_range_x4a.sh` (copy) and `cluster-x4a.out`, `ctest-x4a.log`, `build-x4a.log`, `build-h1.log`, `build-leak.log`. The X3 probes are untouched.

## Not run

- Docs E2E (339 requests): the read corpus, the corrupt-shard probe and the cluster run cover the changed code; the developer's 576 of 576 was not reproduced.
- WAL replay's `execute_line` (`wal.cc:519`, the `engine().revision()` comparison): the kill -9 restart in `p3_paths.py` loses the uncommitted writes and logs no replay, so only `DatabaseWAL`'s constructor (uuid) and `get_revision()` ran. The edit there is one expression; replay with a pending WAL needs a deterministic harness that I did not build.
- `qdiff.py` (2,007) and `qnf.py` (104) query corpora: X4a changes no search path, and `Shard::db()` still serves `match`; the X3 reviews hold these.
- TSan, GCC, Linux, FreeBSD, arm64, LeakSanitizer (not available on this host), a 3-node cluster, failover, benchmarks, libFuzzer (no decoder was added).
