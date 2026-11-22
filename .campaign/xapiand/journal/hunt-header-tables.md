# hunt-header-tables: journal

State: waiting on jobs 261121-202620-10935d (sizes base), 261121-202624-066a03 (sizes head), 261121-202633-71dbfb (A2 diff), 261121-202637-5516f1 (A3 startup), 261121-202645-79e453 (A1 suite)
Quest: .campaign/xapiand/quests/hunt-header-tables.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Experiment 1 (hypothesis: `inline const` removes every per-TU copy and initializer of Xapiand's own
header tables): b853eefa0. Size, diff, startup and suite jobs queued.

Confirmed on master (6cffdc7af), by a transitive `#include` closure over src (grep, so an upper bound):
- `database/data.h`: 12 `static const ct_type_t` + `msgpack_serializers`, in 26 TUs.
- `multivalue/keymaker.h`: 7 namespace-scope `const std::string` (internal linkage; `SERIALIZED_*`
  run `Serialize::floating` at startup), in 4 TUs. Not in the quest text; same defect, in file set.
- `phonetic.h` only includes the soundex headers: 7 `static const` map/vector tables, 4 TUs.
- htm.h's `start_vertices[6]` / `start_trixels[8]` (`const`, internal linkage): 35 TUs.
- No other namespace-scope const objects in src headers.

## Next

1. Read 10935d/066a03/71dbfb/5516f1/79e453; fill Evidence; write docs/dev/header-tables.md (A4) and
   the htm and soundex rows in .campaign/xapiand/hunt.md.

## Blocked on

Nothing.

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

- No address comparisons rely on internal linkage: every use of the ct_type_t objects compares by
  value (`==`, `!=`); `&json_type` etc. only build pointer lists dereferenced for value compares.
- Inline variables keep order: each is defined after the ones it uses in the same header, so
  partially-ordered initialization runs `json_type` before `msgpack_serializers` in every program.
- keymaker.h's strings are in scope: same defect, in the quest's file set.
