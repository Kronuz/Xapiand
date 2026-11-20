# dsl-not-wrap: QueryDSL: _not builds the same tree on every toolchain

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status dsl-not-wrap`.

## Outcome

`_not` builds the same query tree on every toolchain. Today `{{ x }}` in QueryDSL::create_exp_op_dsl (src/query_dsl.cc:1321) is parsed as a nested list on macOS/Linux ({"_not":[[x]]}) and a flat list on FreeBSD ({"_not":[x]}); build it with an explicit push_back.

## Acceptance criteria

- A1 A new test pins the `_not` tree (and every other `{{ }}` construction in query_dsl.cc found by grep) to one shape and runs on all CI platforms; it fails without the fix on at least one toolchain (mutant job or the FreeBSD CI row)
- A2 tests/test_coverage_core_query_dsl.cc:428's platform workaround, if any, is removed or noted for coverage-core-small's owner
- A3 The suite passes and CI is green on all platforms

## Contracts

No API change.

## Decisions

- From hunt.md; filed by the lead 2026-11-19.

## May land early

None.
