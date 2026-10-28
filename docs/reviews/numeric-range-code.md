> Status: CURRENT (2026-10-27) code review of `fix/numeric-range` (Xapiand:76097e087, Xapiand:3dc64ef63, Xapiand:96edfbdc4) against master Xapiand:17e11d2cf. Reviewer: code reviewer, fresh context.

# Numeric range terms: code review

## Verdict: APPROVE WITH CONDITIONS

The decomposition is correct: about 47 million independent never-under-cover checks, 0 wrong, ASan+UBSan clean, and my mutant is
caught. On every value-slot field, every closed range I tried returns exactly what the value source alone returns. The one thing
that stops me from approving outright is the terms-only path: a terms-only field answers a wide or open-ended range with an empty
query, which matches nothing, silently. That is a documented-nowhere data loss of the same kind the branch fixes elsewhere.

Blocking (CR1 only):

1. CR1: a terms-only field returns no documents for any range wider than 4096 coarsest buckets and for every open-ended range.

Conditions: fix CR1 (reject loudly or return a sound superset, see below); correct the mechanism doc where it states what a
terms-only field returns. Everything else is a non-blocking note.

## Evidence

Worktree `~/code/Kronuz/Xapiand-wt/review-numeric-range` (detached at `fix/numeric-range`, Release build with `BUILD_TESTS`,
deleted at the end). Probes are in its `.scratch/`.

1. Built `bin/xapiand`, `xapiand_test_range_terms` (321553 checks pass) and `xapiand_test_generate_terms` (12 checks pass).
2. `.scratch/brute.cc`: my own oracle for `range_terms::decompose`, with the index side re-implemented from
   `GenerateTerms::integer`/`positive` using the real `math.hh` `modulus` (INT64_MIN handled without the overflow). For random
   ranges (signed and unsigned, limit-hugging, power-of-two, small, wide, inverted), 18 accuracy lists (default; non-nested like
   {3,5,11} and {7,100}; single; huge like {2^63}, {2^63+1}, {10, 2^64-1}, {2^63, 2^64-1}), budgets 1, 2, 3, 5, 16, 128, 4096, it
   samples every range end, every emitted bucket's edges and randoms, and checks that some emitted bucket equals the index term of
   the value. Four seeds of 300000 iterations: about 47 million checks, 0 under-cover, 0 over-budget, and every `false` return is a
   range of more than `budget` coarsest buckets (the doc's claim holds). ASan+UBSan (-O2, 100000 iterations, 7.5 million checks):
   clean. Mutant (`b < p.hi - 1` in the final widening, `.scratch/mut/`): 500859 bad of 5.7 million, so the probe can see a
   one-bucket under-cover.
3. `.scratch/live_probe.py BIN`: live server, 15 fields (integer, positive, floating, datetime, time, timedelta, two custom
   non-nested accuracy lists, and terms-only twins of each), about 7700 `_range` queries (closed, half-open, points, narrow ranges
   around values, limits). The oracle for value-slot fields is the value source's own answer (open-ended GE and LE queries carry
   no terms, so GE intersect LE is the source-only answer for a closed range); numeric truth is tracked separately. Branch:
   0 mismatches on closed ranges for `n`, `p`, `f`, `d`, `t`, `td`, `c`, `c2`. Terms-only fields: no miss on any closed range
   within 4000 coarsest buckets; every miss is a wide or open-ended range (CR1). Master binary (same three files swapped in, built
   incrementally, then restored): 361/353/288/131/206/501/400/387 wrong of 517 per value-slot field.
4. `.scratch/perf_probe.py` and `.scratch/miss.py`: screen, 200k docs, median of 7 interleaved runs, no hold (see CR2).
5. Single builder (focus item 3): `MultipleValueRange::getQuery` has one caller, `src/query_dsl.cc:898`; the string form of a range
   (`parse_range`) and a remote shard's query both go through `QueryDSL::get_in_query`. No other caller of
   `GenerateTerms::numeric`; no stale copy of the old walk (`Tree`, `get_query` remain only for the geo and datetime users). I did
   not read `remote_protocol_views.cc` (GPL, off limits); the remote side receives the serialised query, which is generic
   `FILTER(source, terms)`.

## Findings

### CR1 (blocking): terms-only fields return nothing for wide and open-ended ranges

`src/multivalue/range.cc` `getNumericQuery`/`getTimeQuery`/`getTimedeltaQuery`/`getDateQuery`, `return query;` in the
`slot == BAD_VALUENO` branch. When `decompose` returns false (more than `TERMS_ONLY_MAX_TERMS` coarsest buckets) or the end is
open, `GenerateTerms::numeric` returns the empty query, and an empty `engine::Query` matches nothing. With a value slot the same
empty query means "no prefilter" and the source decides, which is right; with no slot nothing decides, and the user gets zero
hits and a 200.

Probe: `live_probe.py` with `ONLY=tn,tp,tf,tc,tt,ttd`, terms-only twins of every kind (a field indexed `"_index": "field_terms"`):
integer `[-2^63+1, 2^63-1]` returns 0 of 246 docs, `{_to: 1025}` 0 of 124, positive `[0, 2^64-1]` 0 of 161, floating
`[-9e18, 9e18]` 0 of 78, custom accuracy {7,100,3000} `[-847860, 1099511627777]` 0 of 109, time `{_to: 27889}` 0 of 28. Misses on closed
ranges within 4000 coarsest buckets: 0 in every terms-only field; closed ranges above that: 110 (tn), 133 (tp), 50 (tf),
33 (tc) all empty. Master was also wrong here (it lost buckets and returned partial answers), so this is not a new class, but
the branch now turns every open-ended terms-only range into an empty answer (empty-while-nonempty: tn 229, tp 215, tf 160,
tc 161, tt 124 of 517 on the branch; 107, 103, 164, 134, 128 on master). `docs/mechanisms/numeric-range-terms.md` ("Terms are a
prefilter") says the answer is "the union of the buckets, wider than the range by less than one finest bucket per end" and does
not mention the empty case.

Fix (either is enough; the first is cheapest): when the field has no slot and the decomposition has no terms, throw a
`QueryParserError` ("range too wide for a terms-only field; index values or narrow the range"), or return the set of documents
that have the field (a superset, which is allowed for a prefilter and wrong only in the safe direction). Add one engine test:
a terms-only field, open-ended range, assert it errors (or returns every document with the field). State the behaviour in the
mechanism doc.

Design question for the architect (not a reason to rewrite): is a terms-only numeric field a supported configuration with range
queries at all? If the answer is "no exact ranges on terms-only", rejecting is the honest behaviour.

### CR2 (non-blocking, measured): open-ended ranges run on the value source alone; the coarse prefilter is not recoverable inside `getNumericQuery`

Is an open-ended range unfiltered by terms when coarse buckets could prefilter it? Yes, and the clamp to the type limit cannot
help: `[lo, INT64_MAX]` is about 9e10 buckets of 1e8, far over any budget, so `decompose` returns false by construction. Master's
open-ended case "worked" only by cutting the range at twice the coarsest accuracy, which is the bug (it returned 0 of 187 for
`n >= 999000000`).

Fair comparison (screen, 200k docs, `n` uniform in [0, 1e9), Release, medians of 7 interleaved runs, no hold). The bounded
equivalent `[lo, 1e9)` has the same result set as the open one and does use terms (at most 10 coarsest buckets):

| query | open (source only) | bounded (terms + source) | ratio |
|---|---|---|---|
| `n >= 999000000` (187 hits) | 9.1 ms | 2.0 ms | 4.5x slower |
| `n <= 1000000` (207 hits) | 7.9 ms | 1.9 ms | 4.2x |
| `n >= 990000000` (2000 hits) | 9.7 ms | 3.9 ms | 2.5x |
| `n >= 900000000` (20075 hits) | 13.6 ms | 8.4 ms | 1.6x |
| `n >= 500000000` (99704 hits) | 29.9 ms | 31.2 ms | 1.0x |

So the cost is a scan of the value slot, about 35 ns per document (7 ms over 200k documents), paid on selective open-ended
queries only; from 50 percent selectivity on there is no difference. It scales with the shard's document count (about 0.7 s per
20 million documents for a selective `>=`), and it is the price of being correct: master returned 0 documents for the same
queries in 1 ms. The developer's "wide 5e8 ranges 2.5 s to 3.2 s" is not a regression: `[1e8, 6e8]` is 31 ms on the branch and 28 ms
on master, but master returned 80208 documents where the right answer is 100363 (master 0.35 ms per 1000 hits, branch 0.31);
the slowdown is the extra documents returned.

Cheap change that restores the prefilter: none inside this function. A prefilter for an open end needs the data's extent, and
the engine has it (`db->get_value_lower_bound(slot)` / `get_value_upper_bound(slot)` exist), but `getQuery` has no database in
hand. Clamping the open end to the shard's value bounds when the query is opened (in the source's `init`, or by passing the
bounds into `QueryDSL`) would give `[lo, upper_bound]` and, for data spanning at most 128 coarsest buckets (1.28e10 with the
default accuracies), the same 2 ms as the bounded query. That is a follow-up with an owner decision (cluster case: per shard),
not part of this fix. Record it in `docs/dev/side-findings.md` or the ledger.

### CR3 (non-blocking, pre-existing, found while probing): the value-slot comparison is not exact for some integers

Independent of the terms (these are open-ended queries, so no term filter is built; identical results on master and branch):
`.scratch/one2.py`, positive field holding 2147483647..2147483651: `_from: 2147483649` returns [2147483649, 8796093022209] (misses
2147483650 and 2147483651), `_to: 2147483649` also returns 2147483650 and 2147483651; `[2147483649, 2147483650]` returns nothing.
Integers at 2^31 and above, at least up to 2^43, and negative integers near -2^43 to -2^62, are compared inexactly by the slot
(8 of 517 closed ranges differ from numeric truth for `n` in my corpus, 51 counting the ends, and 15 for dates around 1900).
`sortable_serialise` is a black box to me; the symptom is the slot's serialised order for integers, not the term code. The
developer's corpus (`2**k + d` for k up to 40, ranges drawn from the same values) does not reach it, so the headline "0 wrong of
797" is true for that corpus and is not a statement about integers in general. Worth a separate item: it is the same kind of
silent loss at a different layer.

Also seen, not investigated further: with two custom-accuracy integer fields in one schema (`c` {7,100,3000} and `c2` {3,5,11,1000},
same values), `c` open-ended `_to: 70` misses the ten most negative documents (`live_probe.py`, `ONLY=c,c2`); with `c` alone, or `c`
with `n`, or `c` with `tc`, it does not. Open-ended means no terms, so it is the source or slot handling, not the branch.

### CR4 (non-blocking): the engine-level test does not cover terms-only fields or any open end

`tests/test_generate_terms.cc` queries the terms alone through the engine, which is the right shape for numeric ranges, but only
the closed case. The two behaviours CR1 describes are not in it. Add a terms-only and an open-ended case when fixing CR1.

### CR5 (non-blocking): notes

1. `truncate_saturating` assumes the index-side `static_cast<int64_t>(double)` saturates. For `|f| >= 2^63` and NaN that cast is
   undefined (x86 yields INT64_MIN, arm64 saturates), so on x86 a floating document near 1e300 is indexed under the INT64_MIN
   bucket while the query asks for INT64_MAX. Pre-existing on the index side; the terms are a prefilter, so such a document is
   lost on x86 only for ranges above 2^63. Fix with a saturating cast on the index side when someone is in that code.
2. For terms-only datetime fields the closed-range answer can be far wider than one finest bucket (`FAR-EXTRA td2`: ranges ending
   at 4107542400 return documents 7 to 16 buckets away) because `GenerateTerms::datetime` is unchanged; the mechanism doc's
   "less than one finest bucket per end" holds for numeric kinds only.
3. `accuracy.empty()` returns `match_all()` in `_numeric` (`generate_terms.cc:957`): for a value-slot field that is a safe no-op
   prefilter; for a terms-only field it is a superset including documents without the field. Fine, but say so in CR1's doc fix.
4. A timedelta of more than 99 hours is rejected by the parser ("Error format in timedelta: -9600:00:00.000"), which limits what
   the timedelta terms can be probed with; unrelated to this branch.
5. `RESTORE` drops the first document of an ndjson body (my 200k-document restore holds 199999; `.scratch/miss.py`). The same
   quirk affects the developer's harnesses by one document; it is why my perf counts differ by one from the data. Not related to
   this branch.
6. Scope: `getDateQuery` open ends now bypass the datetime terms (a deliberate fix for `date >= 2040` returning nothing); within
   the design.

## Checks against the criteria

1. Never under-cover for integer, positive, floating, date, time, timedelta: yes for value-slot fields (probe 3), and for the
   pure function across non-nested, huge and degenerate accuracy lists (probe 2). Negatives: the floor/wrap matches the index
   (`modulus` returns `mod - m` for negatives), confirmed by 47 million checks against the real `modulus`. Limits: INT64_MIN,
   INT64_MAX, UINT64_MAX included in the generator. Empty/inverted: empty. Single points: covered.
2. Terms-only: not safe as built, see CR1.
3. Tests that pass without the code: none found; the property test caught my mutant independently of the developer's, and the
   engine test runs the real query over indexed documents.
4. Scope beyond the design: none (the date open-end change is documented).
5. Limits: `MAX_TERMS` 128 and `TERMS_ONLY_MAX_TERMS` 4096 are budgets with a defined fallback; the fallback for terms-only is the
   problem (CR1).

## Probes and artifacts (kept in `~/code/Kronuz/Xapiand-wt/review-numeric-range/.scratch/`)

`brute.cc` (build: `clang++ -std=c++20 -O2 -I<math-src> -I../src brute.cc`; `SEED=n ./a.out 300000`), `mut/multivalue/range_terms.h`
(the mutant), `live_probe.py` (`ONLY=tn,... SHOWN=n`), `live_branch.log`, `perf_probe.py`, `perf_branch.log`, `perf_master.log`,
`miss.py`, `one.py`, `one2.py`, `one3.py`, `one4.py`, `build.log`. The binaries and the build directory were deleted (rebuild: configure with
`-DBUILD_TESTS=ON`, `FETCHCONTENT_UPDATES_DISCONNECTED=ON` and the `_deps/*-src` of a sibling build).
The master-variant binary was built by checking out the master `generate_terms.{cc,h}` and `range.cc` over the review worktree and
then checking the branch versions back in (worktree clean afterwards).

## Not run

1. `ctest` as a whole (I built and ran only `xapiand_test_range_terms` and `xapiand_test_generate_terms`); the docs E2E (newman,
   576 requests); `harness/numeric_range_check.py` and `harness/id_terms_check.py` as shipped.
2. A full ASan or TSan build of the server (my ASan+UBSan run covers `range_terms.h` and `math.hh` only; TSan not needed, no
   concurrency in the change).
3. Re-running the developer's mutants; the old-walk revert of the engine test.
4. Linux, FreeBSD, x86 (CR5 note 1 is by reading), cluster mode and the remote shard path (single node only).
5. A strict or exclusive performance measurement; the table in CR2 is a screen.
