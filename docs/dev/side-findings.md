# Side findings: the missing 2^28 id and the offload test abort

Status: explored on `explore/side-findings` (Xapiand master d0b43fad2); not landed.

## 1. A numeric value in [2e8, 3e8) is missed by range queries

Symptom: `harness/id_terms_check.py` with ids 2^k-1..2^k+1 reports 268435455, 268435456 and 268435457 missing
from `_id` range `0..3000000000` and `55..2147483647`. Every other power of two (2^16..2^32) is found.
Not specific to `_id` and not new with the seam: `GenerateTerms::numeric` is shared by every integer, positive,
time and timedelta field, and this selection code is unchanged by the seam work.

Root cause: `src/multivalue/generate_terms.cc`, `_numeric`. The default numeric accuracies are
100, 1e3, 1e4, 1e5, 1e6, 1e8 (`def_accuracy_num`, `schema.cc:150`), so `max_acc` is 2e8 (twice the coarsest).
For a range that reaches past it the lower walk stops at `lower_end == max_acc` (`lower < lower_end`, exclusive)
and the upper walk stops at `upper_start == max_acc` (`upper > upper_start`, exclusive, the loop in the UPPER
branch). Neither emits the coarse bucket that starts at `max_acc`, so [2e8, 3e8) has no term. 2^28 = 268435456
sits in it; so does every other value in that bucket. Finer levels cannot recover it (their `upper_start`
inherits the same bound), so `[0,250000000]` also loses [2e8, 2.01e8) at the 1e6 level.

A standalone port of `_numeric` with an evaluator (`.scratch/sim` in the worktree, not committed) reproduces the
server exactly: for `[0,3e9]`, 268435455/6/7, 250000000, 299999999 and 200000000 miss; 199999999 and 300000000 hit.
With accuracies {100,1000}, ranges past 2000 miss [2000,3000): same defect, so it follows `max_acc`, not 2^28.

Prototype fix (one character): `upper > upper_start` to `upper >= upper_start`. It over-approximates by one
bucket, which the value-range FILTER removes. Regression test: `test_numeric_bucket_at_max_accuracy` in
`tests/test_generate_terms.cc` (fails 3/3 before, passes after).

Real server, ordinary field `n` (integer, default accuracy; probe `numprobe.py`, 52 ranges over 100 values
around powers of two and bucket edges): 28 ranges wrong on the master-era binary, 7 after the fix. All of the
[2e8, 3e8) misses are gone; the 7 left are the wider defects below (for example `[1,3000000000]`,
`[1999,4294967295]`, `[100000,134217729]` lose the 1e6..1e8 buckets). On the old binary `[0,0]` and `[100,100]`
also miss their own value. Pre-seam: the offending line is from 2019 (blame ce48dfeaa) and is identical at the pre-seam parent
5cce4860d (read from source; no pre-seam binary was run).

### Wider defects (not fixed, need a designer)

The port shows `_numeric` under-covers in more places; the one-character fix does not close them:

1. A range whose start is aligned to the finest accuracy loses its first bucket: `[0,0]` yields no term for 0
   (`initial = lower_start - acc` pushes the bucket before it).
2. Ranges that exhaust `MAX_TERMS` (128) fall back to "initial"/"final" buckets pushed into the finer level's
   list (`level_terms[pos]`), i.e. interpreted at the wrong accuracy: e.g. `[2, 150416086]` loses 42351625.
   In a random sweep of 2000 ranges over five accuracy lists, 1161 still had a missed value after the fix,
   mostly this class (exceeding the term budget) and the `[2002,2002]`-style aligned cases.
3. Because the terms are a FILTER over the value-range source, any missed bucket is a silently lost document.

Recommendation: replace the lower/upper walks by a plain canonical decomposition (cover [start,end] with the
coarsest buckets that fit, over-approximate at the ends, cap by budget by widening to a coarser bucket) and test
it against the port as an oracle over random ranges and accuracy lists.

## 2. `test_offload_followups` aborts on Release

Reproducible: 50 of 50 runs abort on master Release (macOS x86_64, libc++) with
`condition_variable wait failed: Invalid argument`; 0 of 50 after the fix.

Root cause: `tests/test_offload_followups.cc`, `test_last_seen_refreshes_while_worker_busy`. The pool worker
runs `release_future.wait()` on a `std::shared_future` captured by reference. `reactor::Reactor r` was declared
before `release` and `release_future`, so at scope exit the promise and future were destroyed first while the
worker was still inside `wait()` on the (now freed) shared state, and only then did `~Reactor` join the pool.
A condition variable destroyed under a waiter returns EINVAL, which libc++ turns into the `system_error`.
It is a use-after-scope in the test, not in the offload code. ASan's slower timing hides it.

Fix: declare the promises before the reactor so `~Reactor` joins the worker first. Test-only change.
