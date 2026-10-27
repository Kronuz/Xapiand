# Xapiand code review: engine query (X3 step 2, sort keys and spies)

- Date: 2026-10-26.
- Branch: `phase0.5/engine-query-2`, head Xapiand:635bff800, four commits over local master Xapiand:60efd75ef (Xapiand:4bbdbbe9b, Xapiand:dde867bc6, Xapiand:b7c8377ce, Xapiand:635bff800). Local only, nothing pushed.
- Tier: shared API (`engine::KeyMaker` and `engine::MatchSpy` are the seam strata implements), about 200 `Xapian::` references moved, and one fix reachable from a remote peer.
- Reviewed against: `docs/status/engine-query.md` (step 2 criteria, proof, mutant table), `docs/reviews/engine-query-code.md` (house rules). `docs/reviews/calibration.md` does not exist in this repo.
- Reviewer: code reviewer, fresh context. Never opened `src/xapian/`, `src/sortable_serialise.*` or `src/server/remote_protocol_views.cc`; every diff and grep excluded them. Commit 635bff800 is judged by its message and by the cluster run below. One probe run printed a UBSan line naming `sortable_serialise.cc:147` (a path and a line number, not the file's text).

## Verdict: APPROVE WITH CONDITIONS

Nothing blocks landing in the sense of a crash, a leak, a wire change or an answer that differs from the step 1 binary. The migration is mechanical where it should be, the four fixes land, and the behaviour corpus is byte-identical. Three small things belong in one fix round (C1 to C3) because they are claims the code does not meet; the rest are non-blocking notes.

## What I checked and found sound

1. Behaviour drift. The developer's corpus (`.scratch/qdiff2.py`, 2,007 answers: 62 sort forms times 4 queries times 3 paging variants, `sort=` strings, 45 collapse runs, 280 aggregation specs) run against the step 1 head binary (`engine-query/.scratch/xapiand-final`) and the review build of 635bff800: `cmp` identical (`.scratch/e2e/qd2-base.json`, `qd2-new.json`), 0 AddressSanitizer reports, the same four third-party/pre-existing UBSan lines in both logs. The corpus has no `?nearest` or `?fuzzy` search, and CR6 changed exactly that path, so I wrote one (`.scratch/qnf.py`, 104 answers: 8 queries including range, geo and date sources, times 6 modes, times with and without a body sort, plus aggregations, all non-empty): identical (`.scratch/e2e/qnf-base.json`, `qnf-new.json`).
2. Wire forms. Doubles go through `engine::serialise_double`, which calls Xapian's own function (`src/engine/xapian/codec.cc:30-39`), so aggregation results and key maker references are the same bytes. The 2-node cluster check on the committed head (`.scratch/cluster_range2.sh 2`, `.scratch/cluster-rev.out`) is GREEN: range counts 11, 6, 4, 2; sorted searches from the non-owner node (`-v`, `v`, offset, two keys); terms, stats, histogram and nested aggregations. That run only passes with both remote registrations in place, so commit 635bff800 works as committed (a missing key maker registration fails 4 checks and a missing spy 13, per the developer's mutant run).
3. Unit tests: 15 of 15 under ASan and UBSan (`.scratch/ctest3.log`); `xapiand_test_engine_sort` 28,144 checks.
4. Ownership. Every adapter has a live-counter test (`tests/test_engine_sort.cc:394-465`: borrowed, owning, `clone()`, registered prototypes and the copies the registry rebuilds, malformed serialisations). I read every `new`, `release()` and registry call in the diff: `handler.cc:1270-1277` holds the borrowing adapters in `unique_ptr`s declared before the Enquire (destroyed after it), `callbacks.h:114-123` registers by `release()` (key maker) or by clone of a temporary (spy, posting source), and `rebuild()` returns a fresh owner to Xapian. I found no third instance of the `register_key_maker` leak class.
5. CR6. `get_mset(engine::Query)` translates once (`handler.cc:1228`) and passes the `Xapian::Query` to `match()`, which hands the same object to `get_rset(const Xapian::Query&)` for `nearest` and `fuzzy` (`handler.cc:1258, 1265`). The translated query is a temporary that lives to the end of the `return` expression, then is a `const&` down the call; nothing stores it, so nothing crosses a lifetime boundary. Reusing one `Xapian::Query` for the rset match and the final match is safe because the posting source adapter clones per use (the step 1 `clone()` and `release()` fix), and the identical `nearest`/`fuzzy` answers in item 1 show it. The verbose `#xapian_query` field still translates a second time (`search_views.cc:1993, 2124`); that is only for verbose responses and is the ledger's decision 6.
6. Fix 1, unknown aggregation name (`aggregations.cc:305-309`). `Aggregation::merge_results(const char**, ...)` is the only wire path that indexes a map with a peer-supplied name; bucket keys (`bucket.h:470-510`) create entries instead of looking them up. Probe `probe_sort aggfuzz 60000` (mutated 207-byte results of a spy that saw 30 documents, with `_stats`, nested `_values`/`_max`/`_value_count`, `_histogram`, `_range`, `_filter`/`_avg`, merged through `MatchSpyAdapter::merge_results` then rendered with `get_aggregation()`): 54,587 `SerialisationError`, 521 `Xapian::SerialisationError`, 4,892 merged and rendered, no crash, no ASan report. The ledger's revert (S6) aborts the unit test; I reproduced it.
7. Fix 2, saturating decoders (`serialise.h:294-307`). In range the result is unchanged (a plain truncating cast). Out of range: `integer` gives INT64_MIN or INT64_MAX at or beyond +-2^63 and 0 for NaN; `positive` gives 0 for NaN and anything not above 0 and UINT64_MAX at or beyond 2^64. The boundary constants are exact in long double, and `INT64_MAX` itself round-trips (test line 712). On a 53-bit long double (arm64) a stored INT64_MAX rounds to 2^63 and now clamps to INT64_MAX instead of being undefined. Semantics are right; I have no objection.
8. Fix 4, `register_key_maker` leak: closed (item 4; revert S4 fails 2 checks, reproduced).
9. `DocView::terms` (`convert.cc:222-233`): skip-to-prefix then stop at the first non-match. Probe `probe_sort terms` (empty document, and `Aa:3 Ab B` with prefixes "", A, Ab, Ac, Z, B, Aaa): exact, including the empty document. The terms aggregation answers are in the identical corpus.
10. Spot-check of ledger mutants (my copy `.scratch/rmutants3.py`, output `.scratch/rmutants3.out`): S3 (spy adapter drops `merge_results`) fails 1 check; S4 (`register_key_maker` without `release()`) fails 2; S6 (`merge_results` trusts the name) aborts the test binary; S12 (no clamp) fails 1 check plus 3 UBSan lines. All as the ledger says. S10 is the exception (C3).
11. `engine/codec.h`: acceptable for now. It is the right seam: the only GPL include of the migrated files moved into `src/engine/xapian/codec.cc`, and the byte form is the one both ends of a Xapiand connection already speak. A neutral codec later is a wire change between nodes (rolling upgrade needs both forms or a version), so it is an architect item, not a fix here. It must satisfy its own contract first (C2).

## Conditions (one fix round)

### C1. The ledger is stale after commit 635bff800

`docs/status/engine-query.md:9, 57-59` and the "Blocked on" section (`:318`) still say the registration is the lead's and unticked ("it needs the two-line change... only the lead makes"; criterion 2 is half ticked). The commit exists and the cluster run proves it. Tick criterion 2, replace "Blocked on" with the commit, and list Xapiand:635bff800 under Commits. The ledger also says the step 2 binaries were proven with an uncommitted `sed` edit; say that the committed edit was re-proven (this review's cluster run).

### C2. `engine::unserialise_double` leaks Xapian's exception type through the seam

`engine/codec.h` promises "Throws on bytes that are not one", and the ledger (7e) says "all parse failures are `engine::Error(invalid)`". `src/engine/xapian/codec.cc:36-38` forwards Xapian's function, which throws `Xapian::SerialisationError`. `Multi_MultiValueKeyMaker::unserialise` (`keymaker.cc:722-724`) and the adapters' `rebuild()` (`callbacks.cc:36-44`) only map Xapiand's `SerialisationError` and `engine::Error`.

Probe `probe_sort kmfuzz 300000` (a registered key maker with all 11 key kinds, mutated, through `registry.get_key_maker(..)->unserialise`): 295,462 `Xapian::NetworkError`, 3,229 accepted, and 1,309 `Xapian::SerialisationError` escaping unmapped (from the `FloatKey` and `DateKey` reference doubles; the unit test's garbage seed contains an integer, a geo and a Jaro key, so it never reaches a double, and its `catch (...) { CHECK_TRUE(false); }` is not exercised). Same for the aggregation results: 521 `Xapian::SerialisationError` out of `merge_results` in `aggfuzz`.

Not a crash, and master threw the same type from the same call, so the remote server's existing handling is unchanged. It matters because a strata-side codec will throw `engine::Error`, and callers written against the Xapian type will not catch it. Fix: catch `Xapian::SerialisationError` in `engine/xapian/codec.cc` and throw `engine::Error(engine::errc::invalid, ...)`; add a float and a date key to the garbage test's seed so the claim is exercised.

### C3. Fix 3 (signed overflow on integer keys) removes the undefined behaviour but not the wrong order, and nothing guards it

The ledger says `IntegerKey` "wraps in two's complement now" and the review brief asked whether order stays correct at the extremes. It does not when a distance reaches 2^63, because the wrapped difference is a negative key and sorts as the nearest.

Probe `probe_sort extreme` (`.scratch/probes/probe_sort.cc`; every pairing of nine extreme and mid values as one- and two-value documents, nine reference values, forward and reverse, ordered against an exact 128-bit distance): 4,751 order inversions in 35,640 ordered pairs. Two classes:

1. The true distance is 2^63 or more (reference `INT64_MIN` and value 0, reference -5 and value `INT64_MAX`): the key wraps to a negative number and the farthest document sorts first. Single value: `iabs(INT64_MIN)` stays negative (`keymaker.cc:41-42`).
2. The true distance is representable, but the unselected neighbour overflows and wins the comparison at `keymaker.cc:248-250` and `:278-280`: reference `INT64_MIN+1`, document `{INT64_MIN, 5}` has a true nearest distance of 1 and gets a key of about -9.2e18. 739 inversions are of this class.

Both are the pre-existing master behaviour (master wrapped in practice, with undefined behaviour in the source), so no answer changed (item 1 of the sound list), and values near both ends of int64 are rare. But the claim in the ledger and the test (`test_engine_sort.cc:720` pins `INT64_MAX - 4` for value `INT64_MIN`, reference 5, which is the wrapped number) records the wrap as the contract, and revert S10 (signed subtraction restored) survives every check: the build prints one UBSan line and exits 0, because UBSan is not built fatal (`rmutants3.out`: `exit 0; OK: 28144 checks passed; UBSan lines 1`). A test that passes without the fix is the finding. Either:

1. Saturate the subtraction and abs (clamp to `INT64_MAX`; `__builtin_sub_overflow` is one line), and add the order check as a test (pairs from the probe are a ready table); or
2. Keep the wrap, reword the ledger to "no undefined behaviour; order is wrong for distances of 2^63 or more, as on master", and build the sort test with `-fno-sanitize-recover=undefined` so S10 fails.

Option 1 is the better answer for a sort the strata engine will also implement.

## Non-blocking notes

### N1. `make_view` allocates one view per document per callback

`KeyMakerAdapter::operator()` and `MatchSpyAdapter::operator()` build a `XapianDocView` (a heap allocation and a `Xapian::Document` copy) for every matching document, as the ledger discloses ("the lead measures"). A sorted search over a million hits pays that per document in the hot loop. I did not measure (no benchmarks by rule). A stack view or a reusable one per adapter is an easy follow-up if the lead's A/B shows it.

### N2. A NaN reference double from a peer reaches undefined behaviour in the GPL serialiser

The kmfuzz run prints `sortable_serialise.cc:147/151/155/159: nan is outside the range of representable values of type 'unsigned int'` (a mutated `FloatKey` or `DateKey` reference that decodes to NaN, then serialised). A benign conversion in practice and in a file I do not read; pre-existing. A decode-time rejection of non-finite references in `FloatKey::unserialise` and `DateKey::unserialise` would close it on our side.

### N3. Retry loop in `match()` re-feeds the same borrowed spy

On `DatabaseModifiedError` and the other retried errors the loop runs again with the same `spy_adapter`, which accumulates into the caller's spy a second time (master did the same with the raw pointer). Not introduced here; worth one line in the follow-ups because aggregation counts after a retry would double.

### N4. The step 2 `callbacks.h` contract comments are good

`key`, `see` may throw on undecodable stored values and the engine abandons that search; clone per shard. Noted only so strata's implementer reads them; no change.

## Reproduced

1. Build: ASan and UBSan, RelWithDebInfo, `-DBUILD_TESTS=ON -DLTO=OFF`, x86_64, `.scratch/build-asan` in `Xapiand-wt/review-eq` at 635bff800, `quiet light`, -j4.
2. `ctest`: 15 of 15.
3. Corpus: 2,007 identical; nearest and fuzzy supplement: 104 identical.
4. Cluster: GREEN on the committed head.
5. Seam metric (`.scratch/seam.sh`): 38 files, 704 lines, as stated.
6. Mutants S3, S4, S6, S10, S12; probes below.

## Probes and files kept (all in `.scratch/` of the review worktree)

`probes/probe_sort.cc` and `probes/build_probe_sort.sh` (modes `extreme`, `kmfuzz N`, `aggfuzz N`, `terms`), `qdiff2.py` (copy of the developer's corpus), `qnf.py` (nearest and fuzzy supplement), `cluster_range2.sh` (copy of the developer's cluster script), `rmutants3.py` and `rmutants3.out`, `e2e2.sh`, `e2e/qd2-base.json`, `e2e/qd2-new.json`, `e2e/qnf-base.json`, `e2e/qnf-new.json`, `cluster-rev.out`, `ctest3.log`, `build3.log`, step 1 probes untouched (`probes/probe_eq.cc`, `probe_bound.cc`, `flatmap.py`, `qdiff.py`, `rmutants.py`). `xapiand-head` is a copy of the reviewed binary. After the mutant run the sort test objects in `build-asan` were rebuilt from the clean tree.

## Not run

- Docs E2E (339 requests): the corpus of 2,007 answers, the nearest and fuzzy supplement and the cluster run cover the changed code paths; the E2E was run by the developer at the previous commit (576 of 576) and no source other than the registration line changed since.
- GCC, Linux, FreeBSD builds; LeakSanitizer (not available on this host; the live-counter tests stand in); TSan; benchmarks (N1); libFuzzer.
- A three-node cluster, failover, or a remote shard serving a sorted search under load; the 2-node run exercises the remote registration, the serialised key maker and the spy result merge once.
- Arm64: the saturation boundary on a 53-bit long double was read, not run.
