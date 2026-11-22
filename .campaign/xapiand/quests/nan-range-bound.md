# nan-range-bound: A NaN range bound is one behavior on every field type

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine
state is in `state.json`: `campaign status nan-range-bound`.

## Outcome

A NaN range bound is a 400 on integer and positive fields but matches nothing on float and date
fields (query-dsl-fixes review F5, `reviews/query-dsl-fixes-correctness.md`). After this quest a
NaN bound gets one answer on every numeric and date field, in queries and in range aggregations:
a 400 naming the field and the bound, which is what integer fields already do. That narrows what
float and date fields accept, so the contributor confirms first that no documented example or
test relies on NaN matching nothing; if one does, it stops and reports it (the owner decides
narrowing user-visible behavior).

## Acceptance criteria

- A1 A test per field type (integer, positive, float, date) and per form (query `_range`, range
  aggregation): a NaN bound answers 400 with the field named; it fails without the fix
  (`--fails-without` at main)
- A2 The suite passes at the landed code: the `suite` check
- A3 [artifact] The query DSL reference says a NaN bound is refused
- A4 [ci] CI is green

## Contracts

None.

## May land early

Nothing: one landing.
