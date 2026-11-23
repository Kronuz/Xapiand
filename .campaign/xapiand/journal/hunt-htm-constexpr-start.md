# hunt-htm-constexpr-start: journal

State: waiting on jobs 261123-133516-e862f4 261123-133521-fde1c8 261123-133529-8903d0 261123-133531-7b7420
Quest: .campaign/xapiand/quests/hunt-htm-constexpr-start.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline job 261123-132523-45413e at htm:c4577b7 (TU with only htm.h): __StaticInit 192 B, __bss 240 B,
__mod_init_func 1 entry, `__GLOBAL__sub_I_t.cc` building `start_vertices`; start_trixels has none.
Lead ruled (no cartesian change): make both tables `inline` variables. Done at htm:dcc43b8, with
test/start_tables_test.cc (old values hard-coded) and its CMake target.
Jobs: p2 probe (htm.h-only TU, two using TUs, ld -r of the two) before 261123-133516-e862f4 (c4577b7),
after 261123-133521-fde1c8 (dcc43b8); build-test 261123-133529-8903d0 (A1); values test 261123-133531-7b7420 (A3).

## Next

1. Read the probe outputs (logs under ~/.scratch/campaign/jobs/<id>/a1/log); record before/after in Result.
   If clang still registers a guarded init per using TU, say so honestly.
2. Paste `campaign brief` Evidence table, State -> ready for review, push htm branch.
3. Result note: a constexpr Cartesian ctor (cartesian repo) would remove the last initializer; lead files the hunt row.

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
