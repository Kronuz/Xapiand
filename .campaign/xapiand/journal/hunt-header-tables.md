# hunt-header-tables: journal

State: waiting on jobs 261122-002109-cfb3da (A1 suite), 261122-002121-a5a538 (A2 one-job A/B), 261122-211249-714471 (A3 startup)
Quest: .campaign/xapiand/quests/hunt-header-tables.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Experiment 1 (hypothesis: `inline const` removes every per-TU copy and initializer of Xapiand's own
header tables): b853eefa0. Refuted on Mach-O for initializers: one copy per program, but each
including TU keeps its own guarded `__cxx_global_var_init` (no COMDAT), 324 new ones.

Experiment 2 (lead's ruling, msg on 2026-11-22: one definition per table; constexpr where literal,
else extern + one .cc definition; file set widened to data.cc, keymaker.cc): ae96a1309.
A2 rerun as one job: `--build ab.sh head` fetches a new dependency tree and builds head, then
`ab.sh base` builds base ($JOB_REF_BASE) against the same tree; diff.sh now matches `_GLOBAL__sub_I_`
(Mach-O and ELF) and `__cxx_global_var_init`. Experiment 1's tables kept as exp1-{base,head}.sz.

Confirmed on master (6cffdc7af), by a transitive `#include` closure over src (grep, so an upper bound):
- `database/data.h`: 12 `static const ct_type_t` + `msgpack_serializers`, in 26 TUs.
- `multivalue/keymaker.h`: 7 namespace-scope `const std::string` (internal linkage; `SERIALIZED_*`
  run `Serialize::floating` at startup), in 4 TUs. Not in the quest text; same defect, in file set.
- `phonetic.h` only includes the soundex headers: 7 `static const` map/vector tables, 4 TUs.
- htm.h's `start_vertices[6]` / `start_trixels[8]` (`const`, internal linkage): 35 TUs.
- No other namespace-scope const objects in src headers.

## Next

1. Read cfb3da, a5a538, 16faf1; fill Evidence (A1-A3) and Result; update docs/dev/header-tables.md
   with experiment 2's numbers; State "ready for review"; push.

## Blocked on

Nothing. (The mechanism question was ruled by the lead: option 1, with constexpr where literal.)

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261122-002109-cfb3da (suite passed at b853eefa0) |
| A2 | 261121-202620-10935d (base sizes), 261121-202624-066a03 (head sizes), 261121-202633-71dbfb (diff; its sub_I rows read 0 because the pattern is `__GLOBAL__`, the rows are `_GLOBAL__sub_I_`: re-read from the two size tables, numbers in docs/dev/header-tables.md). Confounded: head links LZ4 statically (+17792 B `__text`), base the dylib; rerun needed |
| A3 | 261122-211249-714471: head median 21.29 ms, base 22.04, base A/A 22.19, sd 0.8-1.8, 7 light holds beside it: no slower, within noise, not a speedup |
| A4 | docs/dev/header-tables.md |
| CI | run 44985100739 at 8f26a9510 failed only test_pool under ASan+UBSan (heap-use-after-free in asio io_context execute): the known flake in docs/dev/test-pool-flake.md, unrelated to these headers; other 5 jobs green |

## Result

Not ready. b853eefa0 gives data.h's and keymaker.h's tables one object per program (`__bss`
-13952 B) but, on Mach-O, one guarded initializer per including TU: static-init code +31808 B,
`__gcc_except_tab` +3796 B, startup initializers 153 -> 473, startup time unchanged within noise.
Numbers, the LZ4 confound, and the soundex (~50 KB of init in 4 TUs) and htm (~192 B in each of up
to 35 TUs) rows are in docs/dev/header-tables.md.

## Rulings I made

- No address comparisons rely on internal linkage: every use of the ct_type_t objects compares by
  value (`==`, `!=`); `&json_type` etc. only build pointer lists dereferenced for value compares.
- Inline variables keep order: each is defined after the ones it uses in the same header, so
  partially-ordered initialization runs `json_type` before `msgpack_serializers` in every program.
- keymaker.h's strings are in scope: same defect, in the quest's file set.
- Which form each table took: all 20 are `extern const` with one definition. `ct_type_t` holds two
  `std::string`s and has `clear()`: not literal without changing the type. `msgpack_serializers` is
  assigned to a `std::vector` in search_views.cc (outside the file set), so it stays a vector. The
  `SERIALIZED_*` strings need `Serialize::floating` at run time. The four other keymaker strings
  could be `constexpr std::string_view`, but that changes their declared type (tests compare them
  as strings, every return site converts); one shape for the seven costs 4 initializers in one TU.
- No order hazard: no namespace-scope initializer outside data.cc/keymaker.cc reads them (grep:
  only function bodies, default arguments and function-local statics); msgpack_serializers follows
  the ct_type_t definitions in data.cc, so in-TU order holds.
- A2 numbers come from the two saved size tables and the `size -m` totals in the job logs, not new
  jobs: the existing jobs cannot isolate the change (LZ4 link drift), so a rerun waits for the
  ruling on the mechanism.
