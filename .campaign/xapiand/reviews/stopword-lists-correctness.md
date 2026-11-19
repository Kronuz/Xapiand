# stopword-lists review: correctness

Commit: eae0478d9
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

1. 261119-093906-b28504 (tests at eae0478d9) and 261119-093907-ca11c3 (suite): the unit scenarios (shipped lists vs files, override words, unreadable file, unknown language, changed/missing/unchanged through `Schema::check_stop_words`). I did not rerun them.
2. 261119-093843-4ea7ac (mutant, 16 checks) for the refusal test failing without the check.
3. A5's before/after jobs (f71030, 4abafa, 83563b, 670fdd): not rerun.

The jobs prove the check function and the schema walk in a unit process. They do not run a server, so everything below is probed end to end against the real `xapiand` binary.

## What I checked myself

One script, `.scratch/xapiand/review-stopword-lists-correctness/probe.py` (run by `run.sh`), starts the quest slot's build at eae0478d9 (the "new" binary) and a master build without the fingerprints (the "old" binary) over one data directory, restarting between phases with `XAPIAND_DATA_PATH` set to a directory with no stopwords, a changed `en.txt`, a reordered CRLF copy of `en.txt`, an empty one, a partial one, one with a BOM, `EN.txt`, a changed `fr.txt` or `es.txt`, an unreadable `en.txt`, and a directory named `en.txt`. Final run: job 261119-101459-0d2b34 (exit 0, its expectations all met; the observations below are its `info` lines). Earlier runs 261119-100849-9d2026, 261119-101046-6b394d and 261119-101218-5d0d94 fixed my own probe mistakes and gave the same results.

1. Refusal paths, list changed (`en.txt` = `the quick`), index `ix` built with the shipped list: SEARCH, COUNT, PUT, UPDATE, DELETE of a document, GET and INFO of a document, and a RESTORE into `ix` (per item) are all refused with an error naming the list and both fingerprints. INFO and DUMP of the whole index still answer, which is right: they read no list. Indexes with other languages (`es`, `nest`, a language `none`) stay usable, so the refusal is per list. A fresh index built under the changed list works. Restore and bulk writes go through `prepare`, so they check; the CLI `--restore` shares that path (not run).
2. A single-index request checks on every handler path (`DatabaseHandler::get_schema` is the one call site, `handler.cc:376`). A request over several indexes parses its query with one index's schema and checks only that one: finding C1.
3. Cache: the lists are process-wide `static const` (overrides read once, shipped lists immutable), so a list cannot change inside a process; an edited override file is read at the next start, and the thread-local cache is gone with the process. The key is the `shared_ptr<const MsgPack>` identity with a `weak_ptr` guard, so a freed schema's address cannot match a new one, and a schema that changes is a new object. The cache is skipped when the schema has a mutable copy or anything left to adopt. No stale entry is reachable.
4. Adoption: with the old binary I created `old` and `old2` (no fingerprint). Under the new binary, SEARCH, COUNT, INFO and GET of `old` work read-only with the list changed, and a later restart under the shipped list still opens it, so a read-only open adopts nothing. A PUT adopts the list current at that moment (`old` took the changed list's fingerprint; `old2` took the shipped one) and persists it: the next start under the other list refuses both, and a PUT to `old` under the shipped list is refused, so a recorded fingerprint is never overwritten.
5. Overrides: same words in another order, with repeats and CRLF give the same fingerprint (opens, no refusal). Empty, partial and BOM files change the fingerprint and refuse existing indexes. An unreadable `en.txt` stops the node at start (exit 70). `EN.txt` is silently another name (observation N4). A custom name that equals a shipped one replaces it (documented).
6. Unknown languages: over HTTP an unknown `_language` is a 400 from `process_language` ("'xx' is not supported") before the new code runs, because every accepted name canonicalizes to one of the 18 lists or to none. The new throws in `validate_required_data`, `text.cc` and `generate_terms.cc` are therefore defensive; `errc::invalid` maps to 400 (`http_utils.h:106`) and `text.cc` is covered by the unit test, `generate_terms.cc:534` is not (N2).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | defect | A search over several indexes checks only the index whose schema its query is parsed with (the first in sorted order). The others are searched with a list that may differ from the one they were built with, answering 200. `zz` was built with the shipped list, `aa` under the changed list: `SEARCH /zz/` is refused (500, fingerprints named), `SEARCH /aa,zz/` and `/zz,aa/` answer 200 with `zz`'s documents. Likewise `/*/` returned `ix`'s documents while `/ix/` is refused, and `/es,ix/`, `/es,wa/` answer 200 with `ix`/`wa` hits. This is a path that opens a stale index and skips `check_stop_words` (the quest's A3 and D137: never silent). | job 261119-101459-0d2b34, lines `B SEARCH /aa,zz/`, `/zz,aa/`, `/zz/`, `/*/`, `/es,ix/`; `handler.cc:376` (only call site); `get_mset` (`handler.cc:1259`) takes one schema for all endpoints | Check each distinct index of the search against its own recorded fingerprints, where the batch resolves its indexes (`search_batch` / the endpoint resolution), reusing the per-schema cache so a wide search pays a lookup per index; or, if that cost is unacceptable for 2,000-index searches (the wide-search quest), the owner may instead accept and document that a multi-index search is refused only through the index its query parses with (then C1 becomes a non-blocking note). Test to add: a data directory with index `zz` built under list A and `aa` built under list B, then `SEARCH /aa,zz/` under list B must be refused naming `zz`; it fails without the fix (the probe line flips from 200 to refused). |
| N1 | no | gap | A text field inside a `_namespace` object records no `_language` and no fingerprint: `ns` is stored as `{_namespace: true, _type: text}` (no language), so changing `en.txt` does not refuse `ns` (`H SEARCH ns` 200). Whether the document's own `_language` stops terms at index time inside a namespace I did not verify. If it does, that list is used with no record. | job 261119-101459-0d2b34 (`A GET /ns/`, `H SEARCH ns`) | Either record the fingerprint where a namespace field's language is read, or state in the docs that namespace fields are not covered. |
| N2 | no | gap | The `generate_terms.cc:534` branch and the `validate_required_data` catch have no test. | `tests/test_stopword_lists.cc` (`parse_text` only) | One test calling `GenerateTerms::text` with an unknown language and checking the 400 kind and the language in the message. |
| N3 | no | nit | The refusal names the index as its whole shard list (`xapiand://probe/ix/.__1;...;.__5`) and answers 500. It names the index, but the operator must read the list of shards to find it, and a configuration fault is not an internal error. | any `B SEARCH ix refused` line | Name `unsharded_path` of the first endpoint; consider a 4xx or 503. |
| N4 | no | nit | Override-file edges decide a list silently on a fresh data directory (an existing index is refused, so only new indexes are exposed): a directory named `en.txt` opens and gives an empty list (fingerprint `cbf29ce484222325`, stops nothing); a UTF-8 BOM joins the first word; `EN.txt` (or `en.TXT`) is another name and is ignored with no log line. | job 261119-101459-0d2b34 (`V`, `D(bom)`, `E` lines); `stopper.cc:72-92` | Skip non-regular files with an error, strip a BOM, log a line for a `*.txt` whose name is not lowercase. |
| N5 | no | nit | The user docs say a refused index can be fixed by putting the file back "or re-create it", but `DELETE /ix/` answers 501 and DUMP carries no schema (a RESTORE into a new index has no `_language`), so there is no HTTP way to re-create under the new list. | `B DELETE /ix2/ (whole index)` 501; `A SEARCH rs` | Reword the docs to the file route, or name the data-directory route. |
| N6 | no | note | Upgrade: an index built when the old build found no list (the install copies the lists to a directory nothing reads) stopped nothing, and adoption stamps it with the current built-in list at its first write. Its old documents keep their stop words as terms while new ones lose them. D137 accepts this; the docs' adoption paragraph already says the index "takes the current list's fingerprint". | `old`/`old2` probes | None needed; consider one changelog sentence. |

## Not run

1. A cluster with a remote shard on a node with other override files: the query is prepared and terms are generated on the node that serves the request, which checks its own schema, but I ran no second node.
2. The CLI `--dump` / `--restore` (same `restore_documents` -> `prepare` path, not driven).
3. Concurrent first writers adopting the same old index; TSan; Linux, FreeBSD, arm64 (CI's).
4. Whether namespace fields stop terms at index time (N1).
5. Cost of C1's fix on a 2,000-index search.

## Re-check

Commit: b34001019
Verdict: APPROVE

Scope: `git diff eae0478d9 b34001019 -- src tests docs`. The slot build I probed is at 769913970, whose `src` is identical to b34001019 (journal-only difference). My queued job 261119-124627-5dba90 waited on `quiet light` behind a load of about 50 and was cancelled; the probe ran directly (one node, a single light process) against that build, so it is a probe, not evidence. Scripts: `.scratch/xapiand/review-stopword-lists-correctness/recheck.py` (with `EXTRA="--schema-pool-size 1" EVICT=3` for the second run).

1. C1 closed. `get_mset` now checks each other distinct index through `Schema::check_cached_stop_words` (no adoption) and rethrows only `StopWordsMismatchError`. Evidence relied on: the contributor's mutant 196ab6 (fails at the revert), the probe 385e6d and the suite d249fa; I did not rerun them. A mismatch is never added to the set (`add` runs after the check, which throws first), so a stale index is refused on every search.
2. N2-N5 closed by reading the diff: the `GenerateTerms::text` unknown-language test, 409 through `catch_http_errors` (test asserts status and that the message names the index and both fingerprints), the index named by `unsharded_path`, directory (`is_regular_file`) and BOM handling with scenario tests, and the docs text (409, file route, no HTTP re-create).
3. Remembered-index set, correctness: `AgreeingIndexes` takes one mutex for `has`, `add`, `size` and `reset`, so it is safe across threads; two searches checking the same index at once both add it, which is harmless. Eviction clears the whole set when `size >= limit`, so a forgotten index is merely rechecked and nothing false is kept; `tests/test_stopword_search.cc` runs `reset_agreeing_indexes(2)` over 4 and 5 indexes for three rounds, checking `size <= 2` and that `zz` is refused every round (the bound and a refusal after eviction both hold).
4. Staleness, the direct write: the journal's list holds for every route except one. `PUT /.xapiand/indexes/<ix>` with a hand-edited `schema` is accepted (204), bypassing the `_stop_fingerprint` validation. In the same process the schema cache keeps serving the old schema, so nothing changes for a while (`/zz/` 200). Once the cache turns over (`--schema-pool-size 1`, other indexes touched), `/zz/` is refused (409, naming `0000000000000000`) while `/es,zz/` still answers 200, because `zz` is remembered by path. After a restart both refuse. So the set can outlive a reload of the schema doc, for the process.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | gap | A remembered index is not rechecked after its schema is reloaded, so a hand-edited reserved schema doc is caught by single-index requests but not by multi-index ones until a restart. It takes a write to the reserved indexes database (an operator, inside the trusted-cluster model) and bypasses all schema validation anyway. | `recheck.py`, run with the slot build at the commit's `src`, `--schema-pool-size 1`: `SEARCH /zz/` 409, `SEARCH /es,zz/` 200 | Key the set by the cached schema's identity and flags (it changes on reload) instead of the path, or accept and add one line to the mechanism note. The journal's "no in-process path" claim should say "except a direct write of the foreign schema document". |
| R2 | no | nit | `get_mset`'s `catch (...)` around the other indexes' check also swallows a cancellation or budget error thrown inside the load; the search then continues to its own next check. I did not trigger it. | `handler.cc` (the loop in `get_mset`) | Rethrow what the search's budget raises, or narrow the catch to the errors a missing index raises. |
| R3 | no | note | Past 65,536 indexes a wide search clears the set each time it fills and pays the reload cost again; correctness is unaffected, only the A5-style cost on a very wide search. | `AgreeingIndexes::add` | None needed. |

Not run: the contributor's perf numbers (560-610 ms at 2,000 indexes), TSan on the set, other platforms.
