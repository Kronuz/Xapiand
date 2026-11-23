# split-xapiand-src: journal

State: waiting on jobs 261121-193134-8f7fe3 (sizes at f32e6e627), 261121-193145-0eac8e (diff vs master), 261121-193241-6c463f (suite), 261121-193241-3c22d6 (split check), 261121-193243-bfd940 (A6), and CI at 8d7356121
Quest: .campaign/xapiand/quests/split-xapiand-src.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Performance pass on the A3 size diff (master ccff9356f, the merge base, vs a35f26074; jobs 7e3aa6, 112d6f, 1958d9). Two fixes at f32e6e627: `positive_of` inline in serialize_detail.h, and the unused `query_dsl.h` dropped from the five serialize TUs. Size jobs at f32e6e627: 261121-193134-8f7fe3 (sizes), 261121-193145-0eac8e (diff vs master).

## Next

1. Read 0eac8e: Serialize::positive(string_view) back to 736 B / ~198 insns, positive_of gone or unchanged, the five `__GLOBAL__sub_I_serialize*` well below 13 KB; fill "after the fix" in the A3 table.
2. Read verify (A1, A2) and A6 at the head; answer the split check's new warnings (dropped include, the inline).
3. Lead: the header-table follow-up under Blocked on.

## Blocked on

Nothing. A follow-up for the lead, outside this file set: the per-TU initializers left after the fix come from namespace-scope `static const` objects defined in headers, one copy per including TU (9 TUs at master pay ~13 KB each): soundex's `*_accents`/`*_composed`/`french_prefixes` tables (soundex lib headers, via keymaker.h, phonetic.h), `database/data.h`'s 11 `ct_type_t` and `msgpack_serializers`, and htm's `start_vertices[6]` (htm lib). Recommendation: make each an `inline const` variable (C++17: one object, one initialization program-wide) or a function-local static, in its own library; that removes the copies from every TU, not only these.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-160657-bc3fb8 (suite at a35f26074189) |
| A2 | 261121-160657-e37af0 (split check, cpp-libraries main's split_check.py at a35f26074189); warnings answered under Warnings. Earlier: 245d6f (old checker, 3 errors, answered); 145050-4c6e2a and the two 64676726d632 split jobs cancelled as stale |
| A3 | 261121-160547-7e3aa6 (master), 261121-160552-112d6f (head), 261121-160601-1958d9 (diff). Replaces 261121-121104-e6ee83 / 261121-121123-22c397 and earlier: Mach-O nm sizes are always zero, so those compared names only |
| A4 | artifact: `docs/dev/source-layout.md` lists every new file (5 serialize_*, 5 metrics_*.h, 3 bucket_*.h) and what it holds; processor.cc kept whole with its reason. Read at the merged head |
| A5 | CI at a35f26074189 |
| A6 | 261121-160715-6bf677 (all new headers at a35f26074189) |
| A7 | lead, 2026-11-21: read the split; aggregations cut into bucket and metric families, serialize.cc into exact, guess, scalar and unserialize, with a shared detail header; each piece opens with what it holds |

Answers to the old checker's findings (245d6f):

- `src/serialize_exact.cc:76`, `serialize_guess.cc:60`, `serialize_scalar.cc:60`, `using serialize_detail::positive_of;`: errors in the old checker, warnings in the new. Each is the using-declaration that gives the piece the shared `positive_of` helper, which left serialize.cc's anonymous namespace so three TUs can call it (declared in serialize_detail.h, defined in serialize_exact.cc). No body, name or order changed; call sites stay `positive_of(...)`.
- `src/serialize_detail.h:41` warning, `uint64_t positive_of(int*, std::string_view) noexcept;` (a new prototype, a linkage change): the same helper's declaration. The definition lost its anonymous namespace (a dropped `static`-equivalent), so it is inlinable across TUs only through LTO; A3 measured the symbol sizes under Release+LTO.
- The over-1,000-line notes are other files, not this quest's, except src/lua/processor.cc (1,125, kept whole and justified in source-layout.md).

## Warnings (the fixed checker), each answered

1. Dropped `static` / linkage: one. `positive_of` sat in serialize.cc's anonymous namespace (internal linkage) and is now `inline` in serialize_detail.h as `serialize_detail::positive_of` (f32e6e627), because three pieces (serialize_exact/guess/scalar.cc) call it. Class `static` members and function-local `constexpr static` objects moved with their functions unchanged.
2. New prototype: none since f32e6e627; serialize_detail.h holds the moved body itself, `inline`. A dropped include: `query_dsl.h` in all five serialize TUs (unused; see Rulings).
3. `using serialize_detail::positive_of;` at namespace scope in serialize_exact.cc, serialize_guess.cc, serialize_scalar.cc: keeps every call site `positive_of(...)` as it was; no body, name or order changed.
4. Moved dynamic initializers: none. serialize.cc's only namespace-scope object is `constexpr char UUID_SEPARATOR_LIST`, constant-initialized, now in serialize_detail.h next to the `UUID_ENCODER` macro; the aggregation headers hold class templates only.
5. Hot code across TUs: the Serialize per-type serializers moved into four TUs, and `positive_of` (a strtoull wrapper) became a cross-TU call that LTO did not inline; it is `inline` in the detail header since f32e6e627. The size table below is the answer.

### A3: the size diff, classified (master ccff9356f vs a35f26074; 1958d9)

Sizes are distance to the next text symbol (Mach-O), so they include alignment padding. "Code" is the bytes up to the last non-padding instruction (fnbytes.py, read off the two binaries of jobs 7e3aa6 and 112d6f). Total text: 6,490,416 -> 6,542,960 B (+52,544 B, +0.8%), all of it the four new initializers.

| Symbol | Master -> split | Class | Cost | Action |
|---|---|---|---|---|
| `Serialize::positive(string_view)` | 736 B, 198 insns -> 560 B, 151 insns + call | inlining change: `positive_of` left the anonymous namespace as an external function and LTO stopped inlining it | one call per text-to-uint64 on the indexing path | `inline` in serialize_detail.h (f32e6e627); measured by size, not timing: the fix restores master's code shape |
| `positive_of` (anon -> `serialize_detail::`) | 224 B, 63 insns -> 224 B, 64 insns | moved (renamed by namespace); master also had an out-of-line copy for its other callers | none | - |
| `__GLOBAL__sub_I_serialize_{exact,guess,scalar,unserialize}.cc` | 0 -> 13,248 / 13,184 / 13,216 / 13,152 B | duplicated initializer: header-defined `static const` tables copied into each new TU (disassembly: soundex `french/spanish/german_accents`, `*_composed`, `french_prefixes` via query_dsl.h -> keymaker.h -> phonetic.h; data.h's 11 `ct_type_t` and `msgpack_serializers`; htm `start_vertices`, 6 `Cartesian`) | startup only, plus a heap copy of the tables per TU; no hot path | dropped the unused `query_dsl.h` (f32e6e627) from all five; what stays is data.h and htm, outside this file set (Blocked on) |
| `__GLOBAL__sub_I_serialize.cc`, `_cartesian.cc` | 13,248 -> 13,152; 13,744 -> 13,712 B | the same initializer, less what moved out / padding | none | - |
| 28 `SerializationError::SerializationError<...>` | 144 B or 128 B, one side each | moved: the template arguments carry `__FILE__`'s length (`char const(&)[20]` "../src/serialize.cc" -> `[27]`, `[32]` for the new file names) | none | - |
| `task::when_all<void,void>` `.resume/.destroy`, `run_child<void>` | same bytes, suffix .4551 -> .5230 etc. | moved: LTO renumbered coroutine parts | none | - |
| `decode_on_stack<2>` | 800 -> 816 B, both 783 code B, 228 insns | padding only | none | - |
| `Family<Summary>::Add<...>` | 304 -> 272 B, both 267 code B, 78 insns | padding only | none | - |
| `___cpu_indicator_init.cold` (2) | 32+64 -> 32+32 B, both 27 code B | padding only | none | - |

After the fix (f32e6e627; 8f7fe3, 0eac8e): <fill>


## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

Non-moves of the performance pass (f32e6e627):

- `serialize_detail::positive_of` is `inline` in serialize_detail.h (its body moved there from serialize_exact.cc), as SPLITTING.md asks of a shared helper: out of line it was no longer inlined into `Serialize::positive(string_view)`. serialize_detail.h gains `<cerrno>` and `strict_stox.hh` for it.
- `#include "query_dsl.h" // for QUERYDSL_FROM, QUERYDSL_TO` dropped from serialize.cc and the four pieces: none of the five uses either name, and it brought ~13 KB of header-table initialization into each TU. serialize_exact/guess/scalar.cc include `strict_stox.hh` (for `strict_stoll`, `strict_stod`), which reached them through it.
- Not merging master now: the size diff compares against the merge base ccff9356f; master's later handler.cc/main.cc changes would add unrelated rows. The lead lands with `--merge-main`.

Linkage changes, all to share a helper between pieces (no behavior change):

- serialize: `positive_of` left the anonymous namespace of serialize.cc for `serialize_detail::positive_of` (serialize_detail.h defines it `inline`; the pieces take it with a `using` line). `UUID_SEPARATOR_LIST` and the `UUID_ENCODER` macro moved to serialize_detail.h unchanged.
- metrics.h: five pieces (metrics_base/simple/variance/quantile/stats.h), original order kept. `class Schema;` and the `HandledSubAggregation` forward declaration stay in metrics_base.h. Later pieces include the earlier ones they need.
- bucket.h: `FilterAggregation` moved ahead of `HistogramAggregation` and `RangeAggregation` (into bucket_terms.h): it depends on neither. bucket_base.h (one class template, `BucketAggregation`, 737 lines with the includes) is not split further: a class cannot be split across files without editing it.
- lua/processor.cc: not split (reverted after the SPLITTING.md reread, see Doing); no linkage change there.
- serialize_geo.cc was joined into serialize_scalar.cc (a pure append of its body).
- No namespace-scope object with a dynamic initializer exists in any of the four files; none moved.
- Each piece carries its original's full include block (unused includes are harmless; trimming them would edit moved lines).
- docs/dev/source-layout.md did not exist: created (A4).
