# split-xapiand-src: journal

State: waiting on jobs 261121-115601-758a68 (A6), 261121-114550-5706de (suite), 261121-114550-041735 (split), 261121-114627-30abac, 261121-114629-a6153b, 261121-114635-b0e829 (A3); split check's tool lands with split-check (not landed at 2026-11-21: the split job may fail for that)
Quest: .campaign/xapiand/quests/split-xapiand-src.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Reread against cpp-libraries docs/SPLITTING.md (lead's message): joined serialize_geo.cc (under 100 lines of body) into serialize_scalar.cc; reverted the lua/processor.cc split (its anonymous namespace is one TU's private, mutually dependent state; the cut exported eight symbols and a cross-TU thread_local read, for pieces nobody opens alone). processor.cc is kept whole (1,125 lines) and says why in docs/dev/source-layout.md. metrics_*.h, bucket_*.h and the other serialize pieces stay: each is a class family or concern of 170 to 470 lines. Jobs resubmitted at the new head.

## Next

1. Read suite (A1), split check (A2; tool pending in split-check), A3 diff, A6.
2. If a header fails A6, add the include and rerun.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

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
