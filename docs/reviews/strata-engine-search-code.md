# Code review: strata-engine-search, S2 (Xapiand:6ac9965b1 on phase7a/strata-engine-write)

Reviewer: code-reviewer, independent. Worktree `~/code/Kronuz/Xapiand-wt/strata-search`. Diff read against the
real merge base (Xapiand:b078d5a18; the write branch has since moved, see CR2). Probes, scripts and logs are in
`.scratch/s2-review/` (listed at the end); the two sanitizer build dirs are deleted. `docs/reviews/calibration.md`
does not exist in this tree, so no earlier-misses list was applied.

## Verdict: APPROVE WITH CONDITIONS

The four risks the lead named hold up. A search takes every shard's lock before it reads any and keeps them to the
end, so the documents, statistics and spy values it sees are one moment of the whole index; pruned and exhaustive
collection return the same documents, weights, ranks and estimates on a tie-heavy corpus; no refused shape returns
a hit or reaches a spy on any path I could build. Two things must change before it lands: one request value makes
the pruned path reserve memory proportional to a client number (CR1), and the branch must be rebased onto the moved
S1 head with `pin()` brought up to its failure rules (CR2). Neither changes a result.

## Blocking items and conditions

1. **CR1 (condition): `_limit`/`_offset` is unbounded and the pruned path reserves for it.** `run()` passes
   `k = min(first + maxitems, 2^32-1)` to `prospect::search` (`src/engine/strata/search.cc:526`, `options.k`).
   The pruned collector does `hits.reserve(2 * k + 1)` per `top_collector` (prospect `search.hh:204`), 16-byte
   hits. The handler has no cap (`src/database/handler.cc:1044` takes `value.as_u64()`). Probe
   `ONE=0,4294967295,0,0` (and `ONE=4294967295,10,0,0`) on an 80-document index: 31.5 s and 11.2 GB resident
   under ASan (the shadow memory touches the reserve), against 0.00 s and 38 MB for the same window with a spy or
   descending ids (the exhaustive heap does not reserve). `1e9` costs 6.8 s and 6 GB; `1e8` 0.6 s and 0.6 GB; a
   plain Release `reserve` of that size is lazy on this macOS (`reserve.bin`: 137 GB untouched) but fails with
   `bad_alloc` under Linux heuristic overcommit, which the dispatcher turns into `errc::internal` (a 500) where
   Xapian answers. So one request value gives different behaviour on the two collection paths, and no cap names a
   mechanism. Fix: clamp `options.k` to the snapshot's document count (the sum of `core().doc_count()` over the
   pinned segments); a window cannot return more, so no result changes. Add a test with `maxitems` and `first` of
   `UINT32_MAX` on a small index, bounded in time and memory. The reserve itself is prospect's: record it for
   prospect as well (reserve to `min(2k+1, expected)`).
2. **CR2 (condition at rebase): `Shard::pin()` lacks S1's failure rules.** On `phase7a/strata-engine-write` head
   (Xapiand:d6c2eaa3e) every method that reads the index calls `core.check()` (shard.cc:195, 540, 571, ...), and
   `rethrow_translated(core)` marks a fenced index. `pin()` (`src/engine/strata/shard.cc:934-947`) does neither:
   after S1's failed-reopen state (`index` empty) it dereferences `core.index->segments()` on an empty
   `std::optional`, and an `index_fenced` raised inside `refresh()` is not recorded. When this branch is rebased,
   `pin()` must call `core.check()` after taking the lock and translate with `rethrow_translated(handle_->core)`;
   `strata::search`'s private copy of `rethrow_translated` must go (the ledger already says so). Also recheck
   `term_key_prefix()` against the key prefix S1's `index_options()` now uses (`keys::key_prefix()`): the term
   table's prefix must be the index's, or key leaves and dictionary terms are looked up in different places. A
   re-check after the rebase should run `probe_pin_holds`, `probe_lock_order` and the snapshot test again.

## Findings and answers to the four risks

### Risk 1 and 2: one snapshot, lock order (no defect found)

- **Mechanism read.** `run()` sorts the distinct indexes by `Shard::identity()` (the `Core*`) and `pin()`s each in
  that order (`search.cc:494-502`); a `Pinned` holds a `std::unique_lock` on `Core::mtx` for the life of the
  `pins` vector, which outlives `snapshot`, `terms` and every scorer (declared first, destroyed last). Every
  index mutation (add, replace, erase, commit and the merge inside it, `cancel_transaction`'s reopen, `refresh`,
  `stats`, `doc_count`) runs under that mutex; prospect/strata start threads only inside a call
  (`strata/core/index.hh:2161`, joined before it returns), so no background writer touches the pinned
  `segment_view` pointers or the live `deleted` bitmaps. Shard k is frozen from its pin to the end, so shard k's
  state at its pin is its state at the last pin: the snapshot is a true cut, not an approximation. The refresh in
  `pin()` happens under the lock, before the segments are read. A shard added or closed mid-query is outside the
  contract (the handler holds the checkout across the call, `handler.cc:1206`); a second handle of one index and
  the same Shard named twice dedupe by identity and are counted once per slot.
- **Probe `probe_pin_holds` (deterministic).** A spy blocks inside its first `see()` (all three locks held; the
  search names the shards 2, 0, 1). Nine writers (add, commit, erase on each shard) start: 0 of 9 finish in 700 ms;
  the spy then sees exactly the 60 pre-write documents; afterwards all 9 finish. ASan and TSan: 6 checks, pass.
- **Probe `probe_lock_order`.** Eight searchers over random lists of 1 to 7 entries from 5 shards plus a second
  handle of shard 2 (permutations, duplicates; every third with a spy, every other descending), a writer adding
  and committing at random, and a thread running `begin_transaction`/add/`cancel_transaction` or commit on shard 4
  (the reopen replaces the index under the lock), with a one-second progress watchdog, for 12 s. ASan 351
  searches, TSan 377: no deadlock, no stall, no short estimate, no TSan report.
- **Probe `probe_spy_throw`.** A spy that throws `std::runtime_error` mid-scan: the search raises `errc::internal`,
  and writers and commits on all three shards then complete (the locks are released by unwinding). ASan and TSan pass.
- **Developer's own tests re-run in separate builds.** Whole `test_engine_search_strata` with
  `NO_XAPIAN_COMPARE=1`: ASan+UBSan (Homebrew LLVM 23, rapidjson patched in the scratch tree) 152,736 checks, no
  report; TSan 152,736 checks, 0 `WARNING: ThreadSanitizer`. The ledger says 152,733; the three extra are the
  tests added after the figure was written, not a difference.
- **Lock order: documented, enforced only by one sort.** It is documented in `docs/mechanisms/strata-search.md`
  (item 4), in the comment in `run()`, and in `Shard::pin`'s and `identity`'s comments. It is enforced by nothing
  but the `std::sort` at the single call site: `pin()` is public, and nothing stops a second caller (S3's
  sorters, S4) from pinning out of order or from searching while holding a pin. Since every writer holds one
  lock and takes no other, no cycle exists today; the only cycles possible are among searchers and the sort
  removes them. See CR5 for the re-entry hazard.
- **CR3 (non-blocking, proof strength): the snapshot test cannot fail a check.** `test_snapshot_consistency`
  detects the three pin mutants by crash or deadlock, as the ledger says; its cut invariant
  (c0 >= c1 >= c2 >= c0 - 1) and per-snapshot weights have no mutant that makes them fail. Probe
  `probe_checker_sensitivity` runs the same writer against three separate one-shard searches (three moments):
  1 of 2,781 observations violates the invariant. A logically mixed but memory-safe snapshot would pass the
  statistical test most runs. `probe_pin_holds` is the deterministic version; add it to the test file (it is
  about 60 lines and uses only the test's own helpers).

### Risk 3: pruned versus exhaustive (no defect found)

- **Probe `probe_pruned_vs_exhaustive`.** Three shards, four commits of twelve documents each, built from three
  templates plus random documents so weights tie exactly; then seven early documents per shard replaced by key
  (the new version lands in a newer segment with a lower docid, so segment order is not docid order), three
  erased, two more replaced and three added with no commit (an unflushed buffer). 263 queries (13 directed,
  including `match_all`, AND/AND_NOT/FILTER over it, a scale, plus 250 random trees) against ten windows
  ((0,1), (0,2), (0,3), (1,1), (2,5), (7,4), (0,10), (5,0), (30,7), (0,100000)), each run pruned (no spy) and
  exhaustive with a spy, against an exhaustive ascending reference sliced to the window: 5,260 window checks,
  identical did, weight (bit equal), rank and estimate in all, plus identical document sets to the model and
  descending order equal to the reference re-sorted (weight, did descending). The corpus is tied: 8,014 adjacent
  equal-weight pairs, 159 queries with ties, 38 where the ascending and descending top hit differ. 0 differences.
- The developer's `compare()` windows (`test_engine_search_strata.cc:533`) only run on a corpus with no
  replacements or deletes, and the writes/deletes test compares estimates only, so my probe covers the segment
  order inversion that the suite did not. Worth adding to the suite (CR3 covers the same file).
- The only divergence between the paths is CR1 (memory for large `k`).

### Risk 4: refusals (no defect found)

- **Probe `probe_refusals`.** 99 refused shapes (every unsupported `Op` as a list, binary, append, on an append
  spine, nested under AND/OR/scale/FILTER/AND_MAYBE, behind a missing term, behind an empty query; wildcard, edit
  distance, text, source, PHRASE/NEAR windows and non-term leaves, one-term PHRASE/NEAR over wildcards), each in
  four request forms (plain, descending, `maxitems` 0, `first` 5), with and without a spy, on a 3-shard and on an
  empty 2-shard index: 1,584 searches. Result: 784 `errc::syntax`; 640 `errc::invalid` and 64 `errc::internal`
  raised by `prepare()` (Xapian's own conversion rejects the shape before strata sees it); 96 returned without
  error and each of those had zero hits, zero estimate and a spy that saw nothing (an empty query returns
  before `validate`). 0 searches returned a hit or showed a spy a document. 0 refusals after a spy had seen a
  document. The refusal check precedes the pin, so no lock is taken.
- The ledger's "refused with a client error" holds only for shapes that pass `prepare()`: a PHRASE/NEAR over a
  scale or wildcard leaf is `errc::internal` (Xapian `UnimplementedError`, a 500) before strata runs, on both
  engines. Not an S2 defect; noted for the architect (CR6).

### Non-blocking notes

- **CR4: `percent` converts NaN to int.** `search.cc:549`: `static_cast<int>(score / top * 100.0 + 0.5)`.
  When the top weight is infinite (a scale factor of `HUGE_VAL`, or a finite one large enough that the sum
  overflows; `Query::scale(t, 1e308)` stays finite, `inf` passes `prepare()`), `inf / inf` is NaN and the cast is
  undefined behaviour: UBSan aborts (probe `SCALE=inf ONLY_TEST=probe_edges`), Release returns an arbitrary
  `percent`, which is only shown under `comments`. Guard with `std::isfinite` and clamp to [0, 100]. `-1`, `NaN`
  and `-inf` factors are rejected by `prepare()` (`errc::invalid`); `0`, `1e-320` and `1e308` agree between the
  pruned and exhaustive paths.
- **CR5: a spy (and S3's key makers) run under every shard's lock.** A `see()` that re-enters a pinned shard
  deadlocks (`probe_reenter`: `find_key` on the searched shard from inside `see()` never returns; killed by the
  8 s alarm). No spy in the tree does this (`AggregationMatchSpy` reads only the `DocView`), but the `MatchSpy`
  and `KeyMaker` contract in `src/engine/callbacks.h` should say "do not call back into the engine", and `pin()`
  could keep a thread-local depth in debug builds to turn the deadlock into an assert. Also `pin_of` is a linear
  scan per requested shard: O(n^2) for a multi-index search over thousands of shards (about 6e8 comparisons at
  25,000), while one sorted index into `pins` would be O(n log n). Arithmetic only, not measured.
- **CR6 (design question for the architect): the lock is held for the whole search, exhaustive scans included.**
  A spy or descending search over a large index blocks every write, commit, merge and `stats` call on every
  shard it names until it ends, and `pin()` flushes the buffer into a segment on every search that follows a
  write (D7). The ledger records that T10 replaces this with a snapshot that outlives the lock. Under the
  many-indexes workload and background-work-is-not-free rules the architect should say whether T10 must land
  before the strata engine serves the aggregation spy the server attaches to every body search. Related: the
  `prepare()` mapping noted under Risk 4, and `engine/search.h:122-125` still says the strata engine "raises
  errc::not_available" for what it does not search (the code raises `errc::syntax`; `not_available` would be
  retried five times).
- Scope: nothing beyond the design (S2 adds search, `Pinned`/`pin()`, the dispatcher, spies pulled forward from
  S3 with the reason recorded, the CMake test entry). `harness/strata_search_check.sh` was not run (needs the
  server).

## Probes and files (all in `.scratch/s2-review/`)

- `probe_s2.cc`: the probe tests (`probe_pruned_vs_exhaustive`, `probe_refusals`, `probe_edges`,
  `probe_spy_throw`, `probe_lock_order`, `probe_checker_sensitivity`, `probe_pin_holds`, `probe_reenter`),
  built on the helpers of `tests/test_engine_search_strata.cc` (copied). Select one with `ONLY_TEST=<name>`;
  `probe_edges` also takes `ONE=first,max,spy,desc`, `SCALE=<factor>`, `BIG=1`, `MAXITEMS`, `FIRSTBIG`.
- `cfg.sh asan|tsan`, `bld.sh asan|tsan`, `probe-build.sh asan|tsan`: configure a build dir with its own
  dependency tree (the rapidjson `GenericStringRef::operator=` line patched to `= delete` in the scratch tree
  only), build the existing test target, then compile `probe_s2.cc` with that target's flags and link it.
- `reserve.cc`: the lazy-reserve check behind CR1. `refusals.out`, `full-asan.out`, `full-tsan.out`,
  `cfg-*.log`, `bld-*.log`, `probe-build-*.log`: outputs.
- Run with `NO_XAPIAN_COMPARE=1` (the Xapian engine's matcher asserts/trips ASan, per the ledger).

## Not run

The Xapian comparison (Release build, `NO_XAPIAN_COMPARE` unset); the server end to end and
`harness/strata_search_check.sh`; the docs suite; the developer's `mutants.py` (I added no source mutants: the
deterministic pin probe replaces the "lock released" mutant, but I did not rerun it against one); a Release build
(CR1's memory was measured under ASan, the Release reserve only as a lazy `reserve`); a lock-hold or contention
measurement; Linux and FreeBSD; ThreadSanitizer on the pruned/refusal probes (TSan ran the lock, pin, spy-throw
probes and the developer's whole file); the rebased S1 head (CR2 is read from the S1 diff, not built).
