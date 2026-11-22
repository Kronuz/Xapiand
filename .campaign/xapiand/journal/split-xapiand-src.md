# split-xapiand-src: journal

State: waiting on jobs 261121-160657-bc3fb8 (suite), 261121-160657-e37af0 (split check), 261121-160547-7e3aa6 / 261121-160552-112d6f / 261121-160601-1958d9 (nm sizes, diff), 261121-160715-6bf677 (A6), and CI at a35f26074189
Quest: .campaign/xapiand/quests/split-xapiand-src.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged master again (clean). Added the opening comment to src/serialize_detail.h (A7 gap). The first A3 nm job proved nothing: Mach-O `nm -S` prints size 0 for every symbol (the job log warns), so its diff compared only names. Replaced by nmsz.sh, which takes a symbol's size as the distance to the next text symbol, strips local `(.N)` suffixes, and diffs master vs head (jobs above). The checker job 261121-145050-4c6e2a is still queued; the warnings are answered from the diff below and the split check reruns through `campaign verify`.

## Next

1. Read the nm diff (1958d9): explain each Serialize/aggregation difference under Warnings below; fix the split if a hot symbol grew.
2. Read the split check at the head: an error left is real; a warning not answered below gets an answer.
3. Lead: A7 read.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-160657-bc3fb8 (suite at a35f26074189) |
| A2 | 261121-160657-e37af0 (split check, cpp-libraries main's split_check.py at a35f26074189); warnings answered under Warnings. Earlier: 245d6f (old checker, 3 errors, answered); 145050-4c6e2a and the two 64676726d632 split jobs cancelled as stale |
| A3 | 261121-160547-7e3aa6 (master), 261121-160552-112d6f (head), 261121-160601-1958d9 (diff). Replaces 261121-121104-e6ee83 / 261121-121123-22c397 and earlier: Mach-O nm sizes are always zero, so those compared names only |
| A4 | artifact: `docs/dev/source-layout.md` lists every new file (5 serialize_*, 5 metrics_*.h, 3 bucket_*.h) and what it holds; processor.cc kept whole with its reason. Read at the merged head |
| A5 | CI at a35f26074189 |
| A6 | 261121-160715-6bf677 (all new headers at a35f26074189) |
| A7 | [manual], the lead's |

Answers to the old checker's findings (245d6f):

- `src/serialize_exact.cc:76`, `serialize_guess.cc:60`, `serialize_scalar.cc:60`, `using serialize_detail::positive_of;`: errors in the old checker, warnings in the new. Each is the using-declaration that gives the piece the shared `positive_of` helper, which left serialize.cc's anonymous namespace so three TUs can call it (declared in serialize_detail.h, defined in serialize_exact.cc). No body, name or order changed; call sites stay `positive_of(...)`.
- `src/serialize_detail.h:41` warning, `uint64_t positive_of(int*, std::string_view) noexcept;` (a new prototype, a linkage change): the same helper's declaration. The definition lost its anonymous namespace (a dropped `static`-equivalent), so it is inlinable across TUs only through LTO; A3 measured the symbol sizes under Release+LTO.
- The over-1,000-line notes are other files, not this quest's, except src/lua/processor.cc (1,125, kept whole and justified in source-layout.md).

## Warnings (the fixed checker), each answered

1. Dropped `static` / linkage: one. `positive_of` sat in serialize.cc's anonymous namespace (internal linkage) and now has external linkage as `serialize_detail::positive_of`, because three pieces (serialize_exact/guess/scalar.cc) call it. Class `static` members and function-local `constexpr static` objects moved with their functions unchanged.
2. New prototype: `uint64_t positive_of(int*, std::string_view) noexcept;` in serialize_detail.h, the declaration of that one helper; its definition is the moved body, in serialize_exact.cc.
3. `using serialize_detail::positive_of;` at namespace scope in serialize_exact.cc, serialize_guess.cc, serialize_scalar.cc: keeps every call site `positive_of(...)` as it was; no body, name or order changed.
4. Moved dynamic initializers: none. serialize.cc's only namespace-scope object is `constexpr char UUID_SEPARATOR_LIST`, constant-initialized, now in serialize_detail.h next to the `UUID_ENCODER` macro; the aggregation headers hold class templates only.
5. Hot code across TUs: the Serialize per-type serializers moved into four TUs, and `positive_of` (a strtoull wrapper) is now a cross-TU call. The `nm` comparison (Release+LTO, jobs above) is the answer; result below.

nm result: <fill from 261121-160601-1958d9>

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
