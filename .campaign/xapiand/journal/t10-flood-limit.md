# t10-flood-limit: journal

State: blocked on lead: the count hook already yields each slice; the flood is one C library call it cannot interrupt
Quest: .campaign/xapiand/quests/t10-flood-limit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Task-side test reverted (36f9ddcf6), master merged (158234c8e), prototype jobs cancelled by the lead. Read src/lua/processor.cc and
docs/mechanisms/lua-bounds.md for the Lua yield; stopped before building (see Blocked on).

## Next

1. The lead rules on the options below; then build, then A1 tests, A2 strict sweeps (master and fix), A3 limit, A4 docs, A5 suite.

## Blocked on

The premise does not hold for the flood script. The count hook already exists and already yields at each slice: `hook()` in
src/lua/processor.cc runs every `hook_period` (10,000 instructions), or at the next instruction once a call's work passes
`slice_work` (200,000 units), and `lua_yield`s to the reactor, where `co_await budget->checkpoint()` spends a slice (`:812`). The
flood script is `s = string.rep('a', 1048576); s:find('.-b')`: one C library call (the matcher, 3.2 x 10^6 steps, about 22 ms)
with no VM instruction inside it, so no hook can fire in it (lua-bounds.md item 2, "No uninterruptible stretch passes
max_call_work"; the call is condemned at 1.6 x 10^7 units). Lua can yield a C function only through lua_yieldk with a
continuation, and the matcher's `.-` expansion is a C loop with no restart point. So the call is one stretch however the hook is
tuned, and it runs in the request's first light turn: nothing charged after it, or by a hook, reclassifies that request.
Options (each changes behavior, so the lead's):
1. Class before the stretch: a request about to run a script call enters the heavy lane first (a heavy post before
   `Processor::operator()`, then `enter_light()` after). The GET waits one heavy turn per pass (about one stretch plus the one in
   progress, k = 2), flat in the client count. Cost: every scripted write is heavy-class (one heavy pass per call and per resumption
   of the script), so an honest scripted write waits behind heavy turns as heavy requests do; D1 rejected "a class per route"
   for this reason. Files: src/server/search_views.cc (the three script sites), src/lua/**, maybe a lane awaiter in src/server/lanes*.
2. Cap the stretch: lower `Limits::max_call_work` (16 x 10^6 units, about 22 ms) to a few slices' worth, so a flood call holds a
   reactor about that long; p99 stays (clients per reactor + 1) x stretch, bounded and small but not flat. Cost: a library call
   that charges more than the cap (a linear op over a string beyond about cap bytes, now allowed up to 16 MiB) answers 400 where it
   answers 200 today.
3. Yield inside the library (lstrlib patch: lua_yieldk with a continuation between match attempts and inside `min_expand`): the
   only way to make the call itself 110 slices; a large Lua patch (contrib/lua/xapiand-limits.patch, drift test) and the
   matcher's recursion limits what can be resumed.
Recommendation: 1, with a limit derived from k = 2 stretches (plus the in-progress one). 2 as a cheap complement if the lead
wants less than one 22 ms stretch per heavy turn.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
