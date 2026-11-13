# x7-coverage: journal

State: blocked on restore-mixed-sign-integers: A2's replay needs master to restore the corpora
Quest: .campaign/xapiand/quests/x7-coverage.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Everything committed: harness/restore_corpora.sh (+_gen.py), harness/coverage.sh, TESTING.md, tests/corpora/{source.ndjson,README.md,alpha9/,master/}. A1 job done (ecb5b2). A2's replay waits on the master fix.

## Findings (lead's msg: 01ce7e failed 'cli-master: DUMP differs')

- 730675 (alpha.9 corpus) done: rt.mp 1409249 bytes. 01ce7e wrote master's rt.mp: byte-identical (cmp) to alpha.9's. Decoded by hand (python): 3000 documents, 3000 distinct _id, in shard order (1, 6, 11 ...); so the dump is complete and the write side (ndjson RESTORE then --dump) is fine on both builds.
- The replay fails on the restore of that msgpack stream: COUNT is 1498 of 3000 after HTTP RESTORE (msgpack) and after CLI --restore, both served from disk. COUNT is the server's own number, so not a harness read-back/compare error. Suspect: a msgpack RESTORE stream with documents ~0.5 KB (some 3 KB) loses documents (chunk boundary?); restore_stream_check uses 100-byte documents. Not yet proven: probe job 261111-162310-eb4bbc counts at each stage (ndjson RESTORE, after commit, after restart, --dump, msgpack RESTORE). If msgpack RESTORE loses documents, it is a server bug to file with that probe's numbers.
- coverage job ecb5b2 done: table in its out (all of src/ 60.8% lines, 51.1% branches; engine areas met).

## Next

1. When restore-mixed-sign-integers lands, rerun `campaign job submit --quest x7-coverage --for A2 -- <build master; harness/restore_corpora.sh check build-rel/bin/xapiand>` at the fixed master (merge it into this branch first); both corpora must pass HTTP RESTORE and CLI --restore.
2. Optionally rerun coverage with WORKLOADS="ctest corpora e2e".
3. Paste `campaign brief`'s table, write Result, State "ready for review".

## Blocked on

A2: restore-mixed-sign-integers lands. The probe (job 261111-162310-eb4bbc) showed msgpack RESTORE answers 500 "Negative value of NEGATIVE_INTEGER is not a valid POSITIVE_INTEGER" for 1502 of the 3000 documents (the lead's reading; quest filed by the lead). The harness is right; the corpora are committed.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | job 261111-144409-ecb5b2 (`harness/coverage.sh`, ctest + docs E2E, exit 0), table below |
| A2 | corpora committed (tests/corpora/alpha9, master; README names builds); replay job pending the fix (Blocked on) |
| A3 | `TESTING.md` |

A1's table (ecb5b2, `coverage.txt`; `coverage-files.txt` per file):

```
area                               files            lines target          branches target  verdict
decoders, parsers, serialisation       6    1396/2331        98%      766/1433        95%   59.9% lines,  53.5% branches: below (lines, branches)
collectors, scorers, counting          3     862/894         95%      478/506         90%   96.4% lines,  94.5% branches: met
engine: strata                         2    1189/1299        90%      285/344         80%   91.5% lines,  82.8% branches: met
engine: seam                           5     603/641         90%      355/386         80%   94.1% lines,  92.0% branches: met
server                                 2    1725/2293        90%      772/1595        80%   75.2% lines,  48.4% branches: below (lines, branches)
database                               8    7616/13721       90%     2924/6332        80%   55.5% lines,  46.2% branches: below (lines, branches)
aggregations                           3     351/467         90%      139/196         80%   75.2% lines,  70.9% branches: below (lines, branches)
geospatial                            11     803/2807        90%      432/1367        80%   28.6% lines,  31.6% branches: below (lines, branches)
lua                                    1     500/559         90%      186/238         80%   89.4% lines,  78.2% branches: below (lines, branches)
multivalue, reserved                   4    1182/1679        90%      518/754         80%   70.4% lines,  68.7% branches: below (lines, branches)
core (src/*.cc)                       16    3880/6369        90%     1534/3271        80%   60.9% lines,  46.9% branches: below (lines, branches)
all of src/                           61   20107/33060               8389/16422             60.8% lines,  51.1% branches
```

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- TESTING.md is new at the repo root (none existed here): targets copied from strata/docs/TESTING.md; area-to-file mapping is `AREAS` in harness/coverage.sh (decoders/parsers: engine/strata/document.cc, pattern.cc, engine/codec.cc, field_parser.cc, serialise.cc, y2j/; collectors: strata/search.cc, expand.cc, engine/search.cc).
- Coverage counts .cc files only (as the 7a gate did); the ecb5b2 run used WORKLOADS="ctest e2e" (the corpora replay fails until the fix).
- The master corpus is the dump of the same 3000 source documents; it is byte-identical to alpha.9's, since the dump is a plain MessagePack stream independent of the engine.
