# split-xapiand-src: journal

State: waiting on jobs 261121-115601-758a68 (A6), 261121-114550-5706de (suite), 261121-114550-041735 (split), 261121-114627-30abac, 261121-114629-a6153b, 261121-114635-b0e829 (A3); split check's tool lands with split-check (not landed at 2026-11-21: the split job may fail for that)
Quest: .campaign/xapiand/quests/split-xapiand-src.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All four splits and the layout note are committed (one commit per file). Waiting on jobs.

## Next

1. When the jobs finish: read the suite (A1), the split check (A2, once split-check has landed: `campaign status split-check`; rerun with `campaign job rerun`) and the A3 diff; list each A3 symbol-size difference under Result.
2. Push the branch (CI, A5) once, answer any warnings of the split check below.

A6 (lead's message): no include edits made yet. Job 261121-115601-758a68 compiles the 10 new headers (serialize_detail.h, metrics_*.h, bucket_*.h, processor_detail.h) twice-included at C++20 with the build's flags (script ~/.scratch/xapiand/split-src/selfcontained_new.sh, flags from compile_commands.json after a configure). If a header fails, add the missing include to that piece and rerun.

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
- lua/processor.cc: `detail::State`, `Call`, `current_call`, `printed`, `hook`, `throw_if_condemned`, `collect` and `run` moved from the TU's anonymous namespace into `namespace lua` (processor_detail.h declares them): external linkage now. The redundant forward declaration `void hook(lua_State*, lua_Debug*);` of processor.cc line 113 was dropped (the header declares it). `hook` is defined after `host_print` and `base_chunk` (a reorder) so the anonymous namespace stays one block. `current_call` and `printed` are defined before it. `collect_slice` moved to processor_run.cc, the only file using it. `current_call` is thread_local and now also read from processor_run.cc: a cross-TU thread-local access once per resume, none in the allocator or the library meters (those stay in processor_state.cc).
- No namespace-scope object with a dynamic initializer exists in any of the four files; none moved.
- Each piece carries its original's full include block (unused includes are harmless; trimming them would edit moved lines).
- docs/dev/source-layout.md did not exist: created (A4).
