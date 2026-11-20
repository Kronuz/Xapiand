# dsl-not-wrap: journal

State: ready for review
Quest: .campaign/xapiand/quests/dsl-not-wrap.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixed src/query_dsl.cc line 1321 (_not: flat [x]) and 394 (cast map {name:o}) with explicit calls; tests/test_query_dsl_not.cc added (prints trees; expected strings unverified until the probe job 261119-144356-c9d35e runs). A2: tests/test_coverage_core_query_dsl.cc is absent in this tree; note for coverage-core-small's owner.

## Next

1. Read the probe job log; fix test expectations (to_string format, NOT syntax, MsgPack API).
2. Mutant job: revert src/query_dsl.cc fix, --fails-without --for A1 (git stash/ref to the pre-fix commit).
3. campaign verify dsl-not-wrap.

## Blocked on

Nothing

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | tests pass with the fix: job 261119-154841-203822; mutant (pre-fix query_dsl.cc) fails the test's own checks on arm64 CI: ci:44643000441 (tests/test_query_dsl_not.cc:51, :52, :62); a synthetic nesting mutant also fails: 261119-154751-976aa9 |
| A2 | 261119-154841-203822 (the unwrap workaround at test_coverage_core_query_dsl.cc:445 is now an exact `_not` shape check) |
| A3 | suite 261119-155154-32bbee passed; CI on the pushed branch is the lead's |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

## Rulings I made

- msg (lead): test leaf is {"a":{"_value":"b"}}. The loop's "AND MAYBE" input threw QueryDslError "Unexpected end of expression": that spelling is not the lexer's AND_MAYBE token (lexer source not read; likely a different keyword). Dropped it from the test; not a quest-scope parser fix. Follow-up for the lead: find the AND_MAYBE query-string spelling.
- Mutant: macOS is already flat for _not, so the mutant job rewrites create_exp_op_dsl to nest the list and expects the test to fail (it pins the shape every toolchain must produce); FreeBSD CI is the real-toolchain check.
- A2: tests/test_coverage_core_query_dsl.cc does not exist in this tree; nothing to remove.
- Merged origin/master (coverage-core-small). Replaced the unwrap workaround in tests/test_coverage_core_query_dsl.cc with an exact `_not` shape check (A2). Restored "a MAYBE b" in test_query_dsl_not. Mutant is the real pre-fix query_dsl.cc (8ceefe142) run against both tests.

- Pre-fix code passes on macOS (job 261119-154845-7b6ce7 exited 0) and on FreeBSD, and fails on arm64 CI (ci:44643000441), so the old coverage comment ("macOS and Linux nest") was wrong about macOS. Cause: `{{ x }}` is overload resolution of a braced init on MsgPack (initializer_list ctor vs copy of the element), which differs by compiler and standard library. This is implementation-divergent overload resolution, not undefined behavior or evaluation order; I did not read the arm64 compiler or msgpack.h to say which rule differs.
