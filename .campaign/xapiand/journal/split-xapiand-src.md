# split-xapiand-src: journal

State: waiting on jobs 261121-145050-4c6e2a (A2, split_check from split-check-sources at the merged head 85892a5f343c; queued behind other holds)
Quest: .campaign/xapiand/quests/split-xapiand-src.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged master (clean, no conflicts; master changed none of this quest's files, only src/engine/strata/shard.cc and docs/campaign files, so nothing to re-apply per file). Submitted A2 with the fixed checker; the suite (A1) and A3/A6 evidence stand at the merged head (campaign reuses them: only the merge came since, touching none of the quest's files). Pushed once.

## Next

1. Read 261121-145050-4c6e2a. An error left is real: fix the split (moves only) and rerun. The three old errors are answered below.
2. Lead: A7 read, rerun `check split` with campaign verify after split-check-sources lands.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-121055-04ff19 (suite passed at eab05d251a2d; only the master merge since, none touching the quest's files) |
| A2 | 261121-145050-4c6e2a (split_check.py from split-check-sources, at 85892a5f343c). Failed jobs examined: 261121-114550-041735 and 261121-121055-ba2719 (script absent, exit 2); 261121-123004-245d6f (old checker, 3 errors + 1 warning, answered below). 261121-145048-988eab (old script path, same checker) cancelled |
| A3 | 261121-114627-30abac (base), 261121-121104-e6ee83 (head), 261121-121123-22c397 (diff); cancelled/skipped, superseded: 261121-114629-a6153b, 261121-114635-b0e829, 261121-121108-0d7f54 |
| A4 | artifact: `docs/dev/source-layout.md` lists every new file (5 serialize_*, 5 metrics_*.h, 3 bucket_*.h) and what it holds; processor.cc kept whole with its reason. Read at the merged head |
| A5 | CI after the push |
| A6 | 261121-121111-6d8976 (all new headers, passed at eab05d251a2d); earlier head: 261121-115601-758a68 |
| A7 | [manual], the lead's |

Answers to the old checker's findings (245d6f):

- `src/serialize_exact.cc:76`, `serialize_guess.cc:60`, `serialize_scalar.cc:60`, `using serialize_detail::positive_of;`: errors in the old checker, warnings in the new. Each is the using-declaration that gives the piece the shared `positive_of` helper, which left serialize.cc's anonymous namespace so three TUs can call it (declared in serialize_detail.h, defined in serialize_exact.cc). No body, name or order changed; call sites stay `positive_of(...)`.
- `src/serialize_detail.h:41` warning, `uint64_t positive_of(int*, std::string_view) noexcept;` (a new prototype, a linkage change): the same helper's declaration. The definition lost its anonymous namespace (a dropped `static`-equivalent), so it is inlinable across TUs only through LTO; A3 measured the symbol sizes under Release+LTO.
- The over-1,000-line notes are other files, not this quest's, except src/lua/processor.cc (1,125, kept whole and justified in source-layout.md).

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

Linkage changes, all to share a helper between pieces (no behavior change):

- serialize: `positive_of` left the anonymous namespace of serialize.cc for `serialize_detail::positive_of` (serialize_detail.h declares it, serialize_exact.cc defines it; the other pieces take it with a `using` line). `UUID_SEPARATOR_LIST` and the `UUID_ENCODER` macro moved to serialize_detail.h unchanged. `positive_of` is no longer inlinable without LTO; Release uses LTO (A3 measures).
- metrics.h: five pieces (metrics_base/simple/variance/quantile/stats.h), original order kept. `class Schema;` and the `HandledSubAggregation` forward declaration stay in metrics_base.h. Later pieces include the earlier ones they need.
- bucket.h: `FilterAggregation` moved ahead of `HistogramAggregation` and `RangeAggregation` (into bucket_terms.h): it depends on neither. bucket_base.h (one class template, `BucketAggregation`, 737 lines with the includes) is not split further: a class cannot be split across files without editing it.
- lua/processor.cc: not split (reverted after the SPLITTING.md reread, see Doing); no linkage change there.
- serialize_geo.cc was joined into serialize_scalar.cc (a pure append of its body).
- No namespace-scope object with a dynamic initializer exists in any of the four files; none moved.
- Each piece carries its original's full include block (unused includes are harmless; trimming them would edit moved lines).
- docs/dev/source-layout.md did not exist: created (A4).
