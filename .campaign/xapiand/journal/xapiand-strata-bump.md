# xapiand-strata-bump: journal

State: working
Quest: .campaign/xapiand/quests/xapiand-strata-bump.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pins bumped (CMakeLists: strata ab0e26a6 = main, open-one-flush; prospect fcd9f3e = main, which pins strata
d8ae10b8, an ancestor). A3 test written: tests/test_engine_error.cc
`test_a_damaged_segment_answers_at_first_read` (pool path, real PosixIO: flip a footer byte at size-30 of the one
seg-*.sx, read through `Shard::get_view` inside `catch_http_errors`, then restore the byte and read again).
Configuring locally (quiet light) to run pins_check and the test before submitting jobs.

## Next

1. Run pins_check against the configured .deps; bump any further pin it names (consensus only if it does).
2. Build test_engine_error; see whether the restored file reads (hypothesis: a corrupt read never marks the
   endpoint stale, so the failed segment record sticks in the cached index; the fix would be in
   src/database/shard.cc, outside the file set).
3. Jobs: build-test check (A2), pins_check (A1), A3 test + mutant, push branch (A4).

## Blocked on

Nothing.

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
