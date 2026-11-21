# empty-aggregation: An aggregation with no type, two types or an unknown key is a 400

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status empty-aggregation`.

## Outcome

Every key of an aggregation is read. It must hold exactly one type, with comments (`#...`) and
sub-aggregations (`_aggs`) around it. Otherwise the request is a 400 naming the aggregation or the
key. That covers an empty aggregation, one with only comments or `_aggs`, one with two types, and
one with an unknown key after its type. Before, master answered `{"_aggs":{"x":{}}}` with a 500. It
also stopped at the first type, so a key after it (`_bogus`, a second type) was ignored and the
result silently changed. Valid aggregations, including comments and `_aggs` before or after the
type, dispatch as before. Decision D167; from the serialize-validation explorer.

## Acceptance criteria

- A1 `test_serialize_validation_aggregation_types` refuses each shape in
  `test_typeless_and_multi_type_aggregations_are_refused` with a 400 naming it and accepts the
  comment and `_aggs` placements: passing at the fix and failing at Xapiand:6720a2f5c (master's
  aggregations.cc with the new test, `--fails-without`)
- A2 The suite passes at the landed code: a job

## Contracts

The aggregation DSL accepts less: what D167 lists is now a 400. No other request changes.

## Decisions

- D167: refuse, as the unknown-type refusal already does (owner, 2026-11-21).

## May land early

None.

## Tasks

None: the fix and its test are Xapiand:96d4664ad on explore/xapiand/empty-aggregation (explorer jobs
261121-052406-912df2, passing at the fix, and 261121-052406-933ff7, the test failing at
Xapiand:6720a2f5c).
