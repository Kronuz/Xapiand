# coverage-multivalue: journal

State: waiting on jobs 261119-044052-c8b854
Quest: .campaign/xapiand/quests/coverage-multivalue.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Three test files written (tests/test_coverage_multivalue.cc keymaker; _terms.cc generate_terms; _range.cc range + geospatialrange), never compiled until jobs 2d327a (keymaker), 6819b2 (terms), 0f1a51 (range) finish: read their out/ logs, fix failures (pin real behavior, list bugs below), then full coverage + suite jobs.

## Next

0. Old jobs failed in the build step: coverage_one.sh's `grep error | head -30` showed only asio error_code lines, so the real errors were hidden. Job 261119-023919-7b8d1a rebuilds all three in one BUILD and greps `error:` outside .deps. Fix compile errors it shows, then resubmit.

1. Read the three coverage_one jobs; fix tests; per-file numbers vs 90 lines / 80 branches (area total 1166/1657 lines at coverage-areas 8642ea).
2. Coverage job (harness/coverage.sh, WORKLOADS="ctest corpora e2e", newman on PATH) and suite job at the head; Evidence rows; push; ready.

Lead msg: if 2d327a, 6819b2 or 0f1a51 times out (exit 124), resubmit one light job, --minutes 150, running coverage_one.sh for all three tests in turn (e.g. bash -c with BUILD per test). None had timed out when read.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

## Bugs found (pinned in tests, src/ untouched)

- M1 MIN_STR_CMPVALUE (keymaker.cc) is std::string("\x00"): built from a C string it is empty, not NUL; a descending SerializeKey on a missing slot keys as empty.
- M1 confirmed by the run: `MIN_STR_CMPVALUE` is empty (size 0); a descending string key on a missing slot is "".
- M2 (range.cc getStringQuery): with no upper bound end_s is "" and start_s > end_s, so a keyword/text range with only a lower bound gives an empty query (pinned, line 287 of the range test).
- M3 (generate_terms.cc): geo with global prefixes indexes level pos under acc_global_prefix[pos] but the query looks levels up in reverse; querying with the same global list finds nothing (pinned).

## Run 261119-035511-e1d625 (keymaker 99.2% lines/98.6% br, terms 93.7/86.3, range 91.7/87.9, geospatialrange 92.0/81.2)

18 failed checks, all expectations: centroids must be unit vectors (distance was NaN), quantization 1e-9, lone range ids < 2^48 start with NUL = list magic, stop words need XAPIAND_DATA_PATH, M2, M3. Fixed at 20f148b59fcd; rerun is job 7797bd.
- M3 settled by 7797bd: same global list finds nothing, reversed list finds the document (checks passed).
- M4 (metric_serialize.h SerializablePhoneticMetric::unserialize): reads and drops the encoder code, so a rebuilt phonetic key serializes shorter (13 to 8 bytes soundex english); pinned. Round trip excludes phonetic slots.
- 7797bd: geospatialrange 96.0/93.8; remaining failures were wrong expectations (stop words key "en", geodetic vs geocentric angles, M4). Fixed at ad0c8d0a8149; rerun ce5d85.
- ce5d85: 1 failure (keymaker.cc:466 rebuilt key == plain key). Cause is real bug M5 (StringKey::serialize writes only the metric, not slot/reverse, so a rebuilt string key reads slot 0). Pinned at eeac65125; scalar kinds still round-trip equal. terms 317 and range 138 checks passed.
- Rerun 261119-044052-c8b854 (--minutes 150, same cmd) at eeac65125. Next: if pass, coverage.sh multivalue job + `campaign verify coverage-multivalue`, then Evidence per-file numbers, ready.
