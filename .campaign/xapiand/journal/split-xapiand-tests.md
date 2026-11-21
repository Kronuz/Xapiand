# split-xapiand-tests: journal

State: waiting on jobs 261121-114927-809003 (suite), 261121-114939-1e141a (A3 head ctest -N), 261121-114945-35c995 (A3 base ctest -N at e5f7765d1); split check 261121-114927-58c8e2 waits on split-check landing
Quest: .campaign/xapiand/quests/split-xapiand-tests.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All four files split, one commit each (lua with the CMake rule); layout note written. Pieces were carved by line range with a script, so every moved line is unchanged; verifying with jobs.

## Next

1. campaign verify (suite), A3 ctest -N at base and head, split check once split-check lands, push.

## Blocked on

Nothing (the split check waits on quest split-check, not yet landed).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | suite job 261121-114927-809003 |
| A2 | split check 261121-114927-58c8e2 (split-check not landed; queued) |
| A3 | head 261121-114939-1e141a, base 261121-114945-35c995: diff their ctest -N output |
| A4 | docs/dev/test-layout.md |
| A5 | CI on the pushed branch |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Linkage changes, all listed: helpers and test functions shared between pieces moved from an anonymous namespace or `static` to external linkage in `namespace detail`, declared in each directory's support.h (and tests.h for lua). In test_engine_query the 22 `static void test_x()` became `void test_x()`.
- Default arguments of moved helpers live in the header declaration; the definition drops them (call, build, run, both, t, ranked).
- Local helpers of one piece stay in that piece inside an anonymous namespace (-Wmissing-declarations is on).
- Dynamic initializers stay in the test's own .cc: the lua watchdog and the query test's `data_path` (setenv).
- The lua `forward declaration of other_requests` and `quick` and `short_strings_800k` moved to support.cc (short_strings_800k now `extern const`).
- Piece headers include `../test_harness.h` and `../engine_support.h`: the pieces are one directory below tests/.
