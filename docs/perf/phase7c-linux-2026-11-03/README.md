# Phase 7c gate rows on Linux, 2026-11-03

> Status: HISTORY (2026-11-03) functional rows and a T12 loadtest on one AWS lease; smoke values, not gate
> values (no `quiet` hold, one host, four trials).

The lead ran the 7c gate's functional rows and a T12 loadtest on an AWS `c7a.8xlarge` lease (`strata-gate7c`, 32
vCPU, Ubuntu 24.04, clang 20, RelWithDebInfo) from 2026-11-03 23:29 to 2026-11-04 00:14 UTC, after the dry run
of `docs/gates/phase7c-linux-dryrun.md`. "cur" is Xapiand master at Xapiand:432299ed0 (the bundle
`~/.scratch/xapiand-master.bundle` of 15:26 PST), "pre" the pre-7c baseline commit Xapiand:142b2ca77 of
`docs/perf/phase7c-baseline.md`, both with `XAPIAND_ENGINE=strata`. The drivers are
`~/.scratch/gate7c-{build,func,func2,time,time2,old}.sh` and the results
`~/.scratch/leases/results/strata-gate7c/` and `~/.scratch/gate7c-res/lt/` on the laptop; no ledger records them
(the AWS spend stop of 2026-11-04 00:13 UTC came minutes later). `rows.tsv` is every functional row's exit code:
all pass at cur except T10 Lua bounds under ASan (`a light GET took 134.5 ms while the scripts ran (limit
50.0)`, also on a 240 s rerun) and the first runs of T3 crash (`node died during startup`) and the gate tools'
self-test, which pass (T3 with ASan too) in the rerun after the lead linked `/usr/local/bin/python3` and
installed `bc` on the host; `lua_asan_dry` exited 2 in 1 s and gave no result. `t12-loadtest.tsv` is
`harness/loadtest.py` (20,000 accounts, then 16 clients for 10 s), four interleaved trials per binary: cur
RESTOREs 1,727 to 1,836 documents/s against pre's 10,088 to 10,427 (median 1,800 against 10,244: 0.18x, x7-gate
A12 wants at least 1.0x), and answers 7,351 to 7,528 queries/s against 6,049 to 6,170 (1.22x), p99 4.45 to 4.54
ms against 5.10 to 5.22 ms.
