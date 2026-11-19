# sort-cast-not-wrap: query DSL: cast words in sorts; _not wraps the same on every toolchain

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status sort-cast-not-wrap`.

## Outcome

Two query DSL defects close: (1) a cast word in a sort (`sort: {"n": {"_integer": "100"}}`, `_datetime`) sorts instead of throwing CastError/DatetimeError, because get_sorter passes the cast value, not the whole map (`value = &o`; hunt :187, job 261119-025306-f2d437); (2) `_not` builds the same tree on every toolchain: `{{ x }}` in QueryDSL::create_exp_op_dsl gives {"_not":[[x]]} on macOS/Linux and {"_not":[x]} on FreeBSD (query_dsl.cc:1321); use an explicit push_back.

## Acceptance criteria

- A1 Sorts with `_integer`, `_float`, `_datetime` and `_keyword` cast words return the expected order; tests fail without the fix
- A2 `_not` produces one documented tree shape; a test asserts the exact serialized query and runs on every CI platform (FreeBSD included)
- A3 The coverage-core-small pins for both flip to the fixed values
- A4 The suite passes and CI is green

## Contracts

None beyond the fixed behavior.

## Decisions

- From hunt.md rows; filed by the lead 2026-11-19.

## May land early

None.
