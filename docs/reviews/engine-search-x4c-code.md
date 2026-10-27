# Xapiand code review: engine search, step X4c (Xapian exception handling moves onto `engine::Error` kinds)

- Date: 2026-10-27.
- Branch: `phase0.5/engine-search-c`, head Xapiand:32cdcc3e1 (five commits over local master Xapiand:da26127a3: Xapiand:7622dd9c8 plan, Xapiand:b28875f07 the move, Xapiand:94c212d24 brace and pass-through fix, Xapiand:01d8b6ccb failure-policy test, Xapiand:32cdcc3e1 proof). Local only, nothing pushed.
- Tier: shared API (`engine::Error`, `errc::syntax`, `read_fate`, `Shard::retry_read` are the contract strata implements) plus a large change on every error path (56 files, +1,714/-917 in the diff I could read; 176 catch sites).
- Reviewed against: `docs/status/engine-search.md` (X4c plan, rulings D1 and D2, "Done (X4c)", "Floor of the seam allowlist", Proof), `docs/reviews/engine-search-x4a-code.md` and `engine-search-x4b-code.md` (house rules), and the lead's rulings in the task (400 for syntax errors, the three floor exceptions, D1). `docs/reviews/calibration.md` does not exist in this repo (same as X4a and X4b).
- Reviewer: code reviewer, fresh context. Never opened `src/xapian/`, `src/sortable_serialise.*` or `src/server/remote_protocol_views.cc`; every diff and grep excluded them. Contact with the GPL file: `git grep -c` and match-only `git grep -n -o` of call-site tokens (`catch (...)` types and `THROW(Name`), and behaviour probes over its TCP port. I did not read a line of it.

## Verdict: APPROVE WITH CONDITIONS

The change does what its design says on every path the corpora and the cluster run reach: 18 of 18 unit tests under ASan and UBSan, four corpora (3,287 answers) byte-identical to the baseline binary, the 2-node cluster run GREEN with the 12 saved bodies identical. Two conditions, both small and both with a verified fix.

1. C1 (the lead's ruling): a text-query syntax error still answers HTTP 500. Reproduced; a 4-line change in `http_utils.h` turns exactly those answers into 400 and changes nothing else.
2. C2 (found by probe): Xapiand's own `InvalidArgumentError` lost its Xapian base, but the remote-protocol server throws it seven times and answers any `Xapian::Error` with an error reply. A malformed message now closes the connection instead of answering. A one-line restore returns the baseline bytes.

## Conditions

### C1. A text-query syntax error answers 500; make it 400

Reproduced on both the baseline and the branch (identical answers): `SEARCH /p1/` with `{"_query":{"title":{"_and":"foo AND"}}}` (also `_or`, and `AND foo`, `NOT`, `foo OR OR bar`, `foo AND AND bar`) answers

```
500 {"status":500,"type":"Internal Server Error","message":"QueryParserError: Syntax: <expression> AND <expression>"}
```

The leaf `_and`/`_or` sets the parser's BOOLEAN flag; `Xapian::QueryParser` throws on a dangling operator (confirmed with `x4c/qp_probe.cc`: 18 throws, all of this family). Probe: `x4c/p1_syntax.py` boots a node, indexes three documents and sends 242 malformed bodies and query strings (`p1-base.json`, `p1-new.json`, 0 differences between them; 10 answers are 500). The `?q=` forms never reach Xapian (the boolean parser answers 400 first), and the four saved corpora contain no row that reaches `syntax` (their 15 and 3 500s are `NEGATIVE_INTEGER is not iterable` and `InvalidArgumentError: op not valid...`), so nothing in the evidence caught it.

Cause: `DatabaseHandler::get_mset` calls `engine::prepare(query)` (`handler.cc:1152`) before the checkout, outside the `match` ladder whose `syntax` arm (`handler.cc:1245`) makes the 400. The exception reaches `catch_http_errors` as a plain `engine::Error` of kind `syntax`, which the default arm (`http_utils.h:100-111`) answers 500.

The change (verified, `x4c/syntax400.patch`), in `src/server/http_utils.h`, the `default:` arm of the `engine::Error` handler:

```diff
 				if (const auto* client_error = dynamic_cast<const ClientError*>(&exc)) {
 					http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
 					http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + client_error->what();
+				} else if (exc.kind == engine::errc::syntax) {
+					http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
+					http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.description();
 				} else {
```

This is the right place rather than `get_mset`: the ladder already answers `Bad Request: <description>` for the same kind, so the two paths now agree, and any other caller of `prepare` (search_views.cc:2299/2307, the manager) gets it too. I applied it to a trial build (`x4c/fixbuild.log`) and reran `p1_syntax.py` (`p1-fix.json`): exactly 10 answers change, each `500 -> 400` with `Bad Request: QueryParserError: Syntax: <expression> AND <expression>`; the other 232 are identical. `test_engine_error` then fails two checks, both on the row `QueryParser` (`tests/test_engine_error.cc:327`: old `500`, new `400`), which is the test that pins the old 500 (`MISMATCH QueryParser: old 500 'QueryParserError: Syntax: bad' new 400 ...`).

Required with the change:

1. `tests/test_engine_error.cc`: the `QueryParser` row (line 327) expects 400 and `Bad Request: QueryParserError: Syntax: bad`; keep the old-dispatch copy for every other row (the case table compares old against new, so this row needs its own expected values).
2. One corpus row that reaches it, so the 400 is covered by the byte-comparison: add `{"_query":{"title":{"_and":"foo AND"}}}` to `qdiff2.py` (the baseline's answer for it is the 500 above; expected after the change is the 400).
3. Update the ledger's "Changes in behaviour" item 4, which records the 500 as kept.

Not asked, for the architect (non-blocking, N6): the same reasoning applies to `errc::invalid` raised by a client's query shape: the corpora hold `InvalidArgumentError: op not valid with a list of subqueries` (500, `{"_scale_weight":[...]}`) and `NEGATIVE_INTEGER is not iterable` (500), both mistakes of the caller. `invalid` cannot be mapped to 400 as a kind because the engine also uses it for non-client reasons; the engine would need to tell the two apart.

### C2. Xapiand's `InvalidArgumentError` no longer reaches the remote server's `Xapian::Error` catch

`exception_xapian.h:64` (`class InvalidArgumentError : public ClientError, public engine::Error`) dropped `Xapian::InvalidArgumentError`. The ledger keeps the Xapian base on `SerialisationError` and `NetworkError` because "the remote and replication servers catch `Xapian::Error` to answer a malformed message", and it converted the two throws in `replication_protocol_views.cc` (`throw engine::xapian::InvalidArgument(errmsg)`, lines 426 and 739). It missed that the remote file has the same pattern: `git grep -n -o -E 'THROW\([A-Za-z]+' src/server/remote_protocol_views.cc` gives 7 `THROW(InvalidArgumentError` (lines 331, 811, 837, 849, 870, 876, 929) and 5 `THROW(NetworkError`; its catches (match-only) are `Xapian::NetworkTimeoutError`, `Xapian::NetworkError`, `Xapian::Error`, `catch (...)`. The `Xapian::Error` arm answers the peer with the error and keeps the connection; `catch (...)` is the replication server's `catch (...)` equivalent: an empty exception reply, then destroy and detach.

Probe `x4c/p2_remote.py` (boots a clustered node, connects raw to its remote port, sends an unknown message type twice, e.g. `fe 00`; `p2-base.out`, `p2-new.out`):

```
baseline  reply1 b'\x01\x1d\x01\x00\x1aUnexpected message type -2'   reply2 the same (connection stays open)
branch    reply1 b'\x01\x00'                                          reply2 ERR [Errno 54] Connection reset by peer  (type 7f: EOF)
```

So on the branch the typed error and the open connection are gone; the peer sees an empty exception and a closed socket. The 30-check cluster run does not reach these throws (valid traffic), which is why it is green.

Fix (verified, `x4c/restore_invalid_argument_base.patch`): put the Xapian base back, exactly as the line was before X4c:

```cpp
class InvalidArgumentError : public ClientError, public Xapian::InvalidArgumentError, public engine::Error {
public:
	template<typename... Args>
	InvalidArgumentError(Args&&... args) : ClientError(std::forward<Args>(args)...), Xapian::InvalidArgumentError(message), engine::Error(engine::errc::invalid, message) { }
```

With it, a trial build answers the probe byte for byte as the baseline (`p2-fix.out`: `diff p2-base.out p2-fix.out` empty). `exception_xapian.h` goes from 9 to 11 seam lines (the class line and the constructor initialiser) (update the allowlist; the floor is then the remote file's actual need, not the ledger's guess). HTTP is unaffected: `InvalidArgumentError` is still a `ClientError` (400) and `engine::Error` first.

Add a unit check that locks it, because nothing did: in `test_engine_error`, `static_assert(std::is_base_of_v<Xapian::Error, InvalidArgumentError>)` next to the existing hierarchy checks, or the equivalent for every Xapiand type the seam list says the remote or replication servers throw (`SerialisationError`, `NetworkError`, `InvalidArgumentError`). The same class of regression is possible for any type that loses its base; the list is short.

Same family, lower reach, no condition (N3): `engine::unserialise_double` (`engine/xapian/codec.cc:42`) now throws a plain `engine::Error(invalid)` where `unserialise_double` threw `Xapian::SerialisationError`; `callbacks.cc:rebuild` turns an `engine::Error` into `Xapian::NetworkError`, which the remote server treats as fatal (closes the connection). Only malformed metric bytes reach it. If the owner wants the old answer there, throw `Failure<errc::invalid, Xapian::SerialisationError>(exc)`.

## The lead's rulings

### Syntax errors answer 400

See C1. The developer's reason for keeping 500 (`prepare` runs outside the ladder) is true and is the reason the HTTP layer must map the kind, not the ladder.

### The three floor exceptions are accepted

1. `replication_protocol_views.cc` (6 lines): holds. Lines 429, 439 and 449 catch `Xapian::NetworkTimeoutError`, `Xapian::NetworkError` and `Xapian::Error` and call the GPL `serialise_error(exc)`, which takes a `Xapian::Error`; lines 745-746 catch what the GPL `unserialise_error` rethrows on the client, which is raw Xapian. Neither can move to `engine::Error` without a serialiser in the MIT tree. Its `NotAvailable` catch and the two throws did move, correctly (`replication_protocol_views.cc:336`, `:426`, `:739`).
2. `shard.cc` 6 and `shard.h` 4 (the `Xapian::Document` writers): holds. `git grep -c -E '(add_document|replace_document|replace_document_term)\('` on the remote file gives 3, one per writer; the remote server speaks Xapian's `Document` and `DocumentInfo` and the wrappers (`shard.cc:1364-1381`) are the only place they convert. The size is right (three declarations, three definitions).
3. `exception_xapian.h` keeping a Xapian base: holds for `SerialisationError` (the replication server's `Xapian::Error` arm answers a malformed message that `length.h` rejects) and `NetworkError` (5 `THROW(NetworkError` in the remote file), but the reasoning applies equally to `InvalidArgumentError`, which the ledger cut: C2.

### D1: read sites retry, write sites keep close-and-throw

Verified by reading every call site, not just the policy function. `retry_read` callers (all reads): `handler.cc:804, 850` (the two dumps), `:1483` (`get_docid_term`), `:2344, 2384, 2424, 2461, 2519, 2561, 2663` (the seven `Document` methods), `shard.cc:1733, 1763, 1807, 1838` (`get_docid_term`, `get_view`, `get_metadata`, `get_metadata_keys`). `collect_failure` callers: `handler.cc:1326` and `:1354` with `writes=false` (`get_metadata_keys`, `get_metadata`), `:1386` and `:1870` with `writes=true` (`set_metadata`, `commit`). No write is in a `retry_read` ladder, and the write fan-outs still close and throw on `reopen` (`shard.cc:667-670`). The write ladders in `shard.cc` (`retry_or_throw`, unchanged) are untouched. `get_docid_term` and `get_view` now close on a final `reopen`, as stated (`retry_close`). The old six A' sites' behaviour change is as the ledger says: a `Modified` that escapes a shard's own retries is collected and the next shard tried, instead of closing and throwing at once.

The policy functions are covered (`test_failure_policy`, 97 checks in `test_pool`, on a real opened shard). The call sites are not: see N1.

## What I checked and found sound

1. Build and unit tests: `.scratch/build-asan` (RelWithDebInfo, `-fsanitize=address,undefined`, `-DBUILD_TESTS=ON -DLTO=OFF`, x86_64 macOS, `quiet light`, -j4; `.scratch/build-x4c.log`, no compile error). `ctest -j4` at the head: 18 of 18 passed (29 s); output in the session, not saved (my later trial builds overwrote the binaries; the source tree is restored).
2. Behaviour drift, four corpora, new binary against the saved baseline answers of the same baseline binary (`engine-search/.scratch/xapiand-base`; `e2e/x4b-*-base.json`), `cmp` identical: `qdiff2.py` 2,007 (1,581 x 200, 411 x 400, 15 x 500), `qnf.py` 104, `qread.py` 911 (709/193 x 404/9 x 400), `qsim.py` 265 (`.scratch/corpora_x4c_new.sh`, `corpora-x4c.out`, `e2e/x4c-*-new.json`). Every 404/400/409/500 body is in the comparison. No AddressSanitizer report and no `blocks on database work` line in the node logs; the UBSan lines are the lz4, rapidjson and the `1.18973e+4932` double conversion the baseline log also prints (`qdiff-data-19302.log`).
3. Cluster: `cluster_range_x4b.sh 2` with the head binary (`cluster-x4c.out`): `VERDICT: GREEN`, 30 of 30 `ok`, 0 failures; the 12 normalised saved bodies (`e2e/cl3-x4c-review/`) `cmp` identical to the developer's `cl3-x4c` set (and so to the baseline set).
4. `catch_http_errors`: kind to status is the old table (`doc_not_found`/`not_found` 404, `version_conflict` 409, `not_available`/`reopen` 503, `timeout` 504, `network` 502 for the four strings read from `detail()` else 500, everything else 500 with `description()`, a `ClientError` that is also an engine error 400). The Xapiand classes that were both a `ClientError` and a Xapian type answer as before: `DocVersionConflictError` 409 (was the `Xapian::DocVersionConflictError` arm), `NetworkError` 500, `InvalidArgumentError` and `QueryParserError` 400. `test_http_answers_unchanged` compares 30 values against a copy of the old dispatch; I read the table and the row list: the 500-for-syntax row is the only one that needs to change (C1).
5. `rethrow_translated` and raw Xapian escapes. Order is sound: `Error` passes, then `std::exception` passes (Xapian's exceptions are not `std::exception`; Xapiand's `SerialisationError` is, and must stay a 400), then each Xapian type from most to least derived, `QueryParser` before `DatabaseError`, `Other` for any other `Xapian::Error`. Every `Failure<K, X>` still derives from its Xapian type, so a catch of a Xapian type still matches it; there is no raw Xapian type left that the adapter can throw: every `engine::xapian::Shard` entry point (25 `rethrow_translated` sites in `engine/xapian/shard.cc`) and `search`/`prepare` end in `catch (...) { rethrow_translated(); }`, `DocView`'s eight methods run in `guarded`, and the two unguarded entry points (`index_text_word_breaks`, `makeStem`) do not touch a database. A non-Xapian, non-`std::exception` value (an `int`) passes unchanged (`test_translation_passes_others`). The only raw Xapian exceptions left in the MIT tree come from the GPL serialiser's `unserialise_error` in the replication client (above), which is the floor.
6. `DocView` throws `engine::Error` only: `test_view_throws_engine_errors` reads a view over a closed database through all seven reading methods and checks the kind; I read `guarded` and the two lambdas that call sinks (a sink's own exception passes through by design).
7. Mechanical damage scan (the swapped-brace class). I read the whole diff of `handler.cc` (every converted ladder and missing-is-data catch), `shard.cc`, `schemas_lru.cc` (the two `doc_not_found`/`not_found` switches and the `save_failed` lambda that replaces the old `catch (...)` body: it is called from the new `catch (const engine::Error&)` for every kind but `version_conflict` and from `catch (...)`, and ends in `throw;`, so both old paths keep their revert-and-rethrow), `manager.cc`, `lua/processor.cc`, `index_resolver_lru.cc`, `discovery.cc`, `search_views.cc`, `replication_protocol_*`, `main.cc`, `http_utils.h`. Each `case ... { ... break; }` is balanced, each `if (!is_missing) { throw; }` keeps the old swallow set (`doc_not_found` and `not_found`) and nothing wider, and the rewrapped `NotAvailable` arms (`manager.cc:859`, `discovery.cc:551`, `index_resolver_lru.cc:458`, `replication_protocol_service.cc:118`, `replication_protocol_views.cc:336`) rethrow or log every other kind as before. I found no damage; the corpora and cluster confirm for the reachable paths.
8. `match` ladder (`handler.cc:1215-1255`): same retry set as before (`reopen`, `opening` with `not_found` and `not_available` as its subclasses, `timeout`, `network`, `closed`; no close, as before), `syntax` and the Xapiand `QueryParserError`/`SerialisationError`/`QueryDslError` arms still make the 400, everything else rethrows.
9. `seam_check.sh`: exit 0 (7 files, 156 lines), as the ledger says. After C2 `exception_xapian.h` is 11.
10. Mutants (30 claimed). Spot-check with my copy of the driver (`x4c/mutants_review.py`): "translate Timeout -> Network" is caught by `test_engine_error`; "fate corrupt -> fail" is caught by `test_engine_error` on the rerun (the first run printed BUILD-FAIL while another build of mine shared the tree; I did not chase it). Three of my own, aimed at the claim "no write moved into a retry ladder", all SURVIVE the unit suite (N1). The developer's 30 cover the kind table, the HTTP table and the policy function; they do not touch the call sites.

## Mutant results (`x4c/mutants-review.out`, `mutants-review2.out`)

```
dev: translate Timeout -> Network: caught by xapiand_test_engine_error
dev: fate corrupt -> fail: caught by xapiand_test_engine_error   (first run BUILD-FAIL, rerun in mutants-review2.out)
rev: set_metadata fan-out treated as a read (writes true -> false): SURVIVED
rev: commit fan-out treated as a read (writes true -> false): SURVIVED
rev: get_docid_term ladder never closes (retry_read call removed): SURVIVED
```

## Non-blocking notes

### N1. The unit suite pins the policy functions, not the call sites

The three survivors above: flipping `writes` at `handler.cc:1386` or `:1870`, or dropping the `retry_read` call in `Shard::get_docid_term`, changes no test. They could not be caught by the corpora either, because a real `DatabaseModifiedError` needs `XAPIAND_RANDOM_ERRORS` (off). The developer says so. Follow-up: one test per fan-out that makes the shard's engine throw each kind through `DatabaseHandler` (a fake `engine::Shard` behind the pool is enough), or a source-level check that the two `writes=true` sites stay. The policy table itself is well covered.

### N2. Kinds that now catch more than the Xapian type did

`Shard::get_metadata` and `get_metadata_keys` break out of the ladder on `kind == errc::invalid` (`shard.cc:1806`, `:1837`), where the old code caught only `Xapian::InvalidArgumentError`; `invalid` also stands for `InvalidOperationError`, so a failed metadata read with that error now returns empty instead of throwing. I could not make Xapian raise it from these calls, so it is theoretical; a one-line `reason`/type check or a separate kind would keep the old set.

### N3. Wire-visible, malformed input only

See the end of C2 (`unserialise_double`).

### N4. Log text for a raw Xapian error

`main.cc:1071` (`describe_exception`) and `:1126` lost their `Xapian::Error` arms; a raw one (the replication client's `unserialise_error` result, or a GPL-side throw) is now logged as "Unknown exception!" if it ever reaches them. Add the arm back or translate in the replication client.

### N5. Test hygiene

`test_failure_policy` leaves a `pool_test_policy/` database directory in the working directory it runs from (it appeared untracked in the repo root when I ran the binary there). Use the test's temp directory helper or remove it at the end. Also, the corpora have no row that reaches `errc::syntax` (C1 fixes this for the one path).

### N6. For the architect: `invalid` is one kind for two things

See the end of C1. `errc::invalid` carries both a caller's malformed query (`op not valid with a list of subqueries`, a 500 today) and an internal contract breach; the HTTP layer cannot choose 400 from the kind. If the owner's direction "a client mistake is a 400" is general, the engine needs a way to say which.

### N7. Ledger

The "Docs E2E 339 requests, 576 of 576" run is the developer's (I did not rerun it; see "Not run"). "Changes in behaviour" item 4 changes with C1; the floor section changes with C2.

## Reproduced

1. Build and `ctest`: 18 of 18.
2. Four corpora against the baseline: identical (2,007, 104, 911, 265).
3. Cluster: GREEN 30 of 30, 12 bodies identical.
4. C1: 10 x 500 on baseline and branch, 10 x 400 with the patch, 232 unchanged.
5. C2: baseline typed reply and open connection; branch empty reply and closed connection; trial build equal to baseline.
6. Mutants: 2 of the developer's (both caught), 3 of mine (all survive).

## Probes and files kept (all in `.scratch/` of the review worktree `Xapiand-wt/review-eq`; none deleted)

- `x4c/p1_syntax.py` (malformed text queries, 242 bodies and query strings; `p1-base.json`, `p1-new.json`, `p1-new-before.json`, `p1-fix.json`, `*.out`).
- `x4c/p2_remote.py` (raw remote-protocol messages; `p2-base.out`, `p2-new.out`, `p2-fix.out`).
- `x4c/qp_probe.cc` and its binary (which strings make `Xapian::QueryParser` throw under the DSL's flags).
- `x4c/mutants_review.py`, `mutants-review.out`, `mutants-review2.out`.
- `x4c/syntax400.patch`, `x4c/restore_invalid_argument_base.patch` (the verified fixes; not applied to the branch), `x4c/fixbuild.log`, `fixbuild2.log`.
- `corpora_x4c.sh`, `corpora_x4c_new.sh`, `corpora-x4c.out`, `e2e/x4c-*-new.json`, `cluster-x4c.out`, `e2e/cl3-x4c-review/`, `build-x4c.log`, `configure-asan.log`, `build-asan.sh`.

The source tree is clean (`git checkout -- src` after each trial); the `build-asan` tree is deleted.

## Not run

- Docs E2E (339 requests, 576 assertions) on the head: the corpora, the cluster run and the probes cover the changed paths; the developer's run stands.
- TSan (no concurrency was added; `retry_read`/`collect_failure` run on the caller's thread), GCC, Linux, FreeBSD, arm64, LeakSanitizer, libFuzzer (no decoder or parser added), benchmarks, a 3-node cluster, failover.
- `XAPIAND_RANDOM_ERRORS` injection through the converted fan-outs (the developer's and mine: a real `DatabaseModifiedError` past the shard's own ladder), the random-errors build.
- The remote server's other six `THROW(InvalidArgumentError` sites individually (I could not read what triggers them; the dispatch default proves the class of failure), the malformed-metric-bytes path (N3), `index_text_word_breaks` under `USE_ICU` with CJK input (the ICU path builds and `test_generate_terms` passes; no CJK corpus row).
