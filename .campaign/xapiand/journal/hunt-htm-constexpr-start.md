# hunt-htm-constexpr-start: journal

State: waiting on jobs 261123-132523-45413e
Quest: .campaign/xapiand/quests/hunt-htm-constexpr-start.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline probe job 261123-132523-45413e (queued; host load ~100): compiles `#include "htm.h"` alone,
clang++ 17 -std=c++20 -O2 -DNDEBUG, cartesian pinned 05cab81, at htm:c4577b7; prints `size -m`,
__mod_init_func, nm and the init disassembly. Script: ~/.scratch/htm/constexpr-start/probe.sh DIR.

Findings so far (reading, not measured): `Cartesian(double,double,double,int)` is defined out of line
in cartesian.cc (cartesian repo, outside this quest's file set) and has a non-constexpr
`toWGS84()` branch, so the 6 `start_vertices` need a dynamic initializer per TU unless the table
stops using Cartesian or cartesian gets a constexpr ctor. `trixel_t` holds a `std::string name`.

## Next

1. Read the job's output; if no `__mod_init_func`/init function in t.o, record "none" in Result and stop.
2. Else decide: constexpr table inside htm.h only (plain-double table, no Cartesian ctor) vs a cartesian change (other repo: Blocked on).

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

<Decisions inside the quest's scope, so the reviewer can check them.>
