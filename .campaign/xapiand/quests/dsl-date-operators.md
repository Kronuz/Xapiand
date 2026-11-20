# dsl-date-operators: Query DSL serves _date, _time and _timedelta operators

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status dsl-date-operators`.

## Outcome

Field and global queries accept `_date`, `_time` and `_timedelta` as typed-value operators, as they accept `_datetime`; today they answer 400 "Invalid operator: _date" (src/query_dsl.cc:270, 380, 1404, 1449). D130 (amended), serialize-validation review S1/S2.

## Acceptance criteria

- A1 An HTTP test queries a field with each of `_date`, `_time`, `_timedelta` and finds the matching document; each fails without the fix (mutant job)
- A2 `{"_datetime": "2020-01-02"}` on a new field is served (fixed by serialize-validation, re-check probe 261119-155820-b92bea) and `{"_datetime": {"_date": ..., "_time": ...}}` (re-check R1, a 400 today) is served or refused with a clear message: HTTP tests for both, plus the old 500 case (`{"_float": ...}` into a datetime field)
- A3 The suite passes and CI is green

## Contracts

Query DSL operators (user-visible: adds accepted input only).

## Decisions

- D130 amended 2026-11-19.

## May land early

None.
