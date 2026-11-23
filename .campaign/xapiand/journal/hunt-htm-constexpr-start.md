# hunt-htm-constexpr-start: journal

State: waiting on jobs 261123-135636-c6b4ab 261123-135639-512858 261123-135642-9c642a 261123-135645-812a48 261123-135647-01d709
Quest: .campaign/xapiand/quests/hunt-htm-constexpr-start.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Baseline job 261123-132523-45413e at htm:c4577b7 (htm.h-only TU): __StaticInit 192 B, __bss 240 B, 1 __mod_init_func.
Inline variables (htm:dcc43b8) proved wrong (probe 261123-133521-fde1c8: 277 B guarded init per TU, b+c 565 B, 2 entries; before 261123-133516-e862f4).
Lead ruled the fallback: tables `extern` in htm.h, defined once in htm.cc (already in the htm library target), htm:e51e63e.
Jobs at the extern fix: probe3 (adds htm.o and a b+c+htm.o link) before 261123-135636-c6b4ab (c4577b7), inline 261123-135639-512858 (dcc43b8),
extern 261123-135642-9c642a (e51e63e); A1 build-test 261123-135645-812a48; A3 values test 261123-135647-01d709 (the dcc43b8 run 261123-133531-7b7420 was superseded).
Superseded jobs: 261123-133529-8903d0 (build-test at dcc43b8).

## Next

1. Read the three probe3 logs (~/.scratch/campaign/jobs/<id>/a1/log); put before/inline/extern in Result with commits.
   Expect: htm.h-only TU no __StaticInit/__mod_init_func; htm.o one initializer.
2. Paste `campaign brief` Evidence table; State -> ready for review.
3. Result note: a constexpr Cartesian ctor (cartesian repo) removes the last initializer; lead files the hunt row.
   Also note: the table is now initialized in htm.cc's TU, so a global constructor in another TU that reads start_vertices has no order guarantee (Xapiand's users are functions, not global initializers).

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
