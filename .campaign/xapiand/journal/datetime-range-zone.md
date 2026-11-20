# datetime-range-zone: journal

State: waiting on jobs 261119-163517-332cfd (test passes), 261119-163518-7d7ef0 (mutant, old source must fail), 261119-163519-1b1b09 (suite)
Quest: .campaign/xapiand/quests/datetime-range-zone.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix at 68e222ce4 in src/datetime_msgpack.cc (the library's computeTimeZone is fine; datetime pin b5d46a8 matches /Users/kronuz/code/Kronuz/datetime HEAD): object _hour/_min/_sec range-checked (0-23/0-59/0-59, negatives out of range, error "'_hour' is out of range"); a `_time` string's zone is deferred in a Zone struct and applied after the whole map is parsed. Pins flipped in tests/test_coverage_core_datetime.cc: test_object_time_is_not_range_checked -> test_object_time_is_range_checked; test_time_strings' datetime-zone-order pin (`_time` before `_date`: 2020-3-1 23:30 -> 2020-2-29 23:30). New test_zone_is_the_same_instant (string and map, +02:00, -05:00, +05:30, -03:30).

Suite at 68e222ce4 failed (test_coverage_core_datetime, test_coverage_serialize aborted): a signed MsgPack integer (JSON numbers) is not necessarily negative, and my check threw on every NEGATIVE_INTEGER type. Fixed at 18395fff1 (value checked, not type); jobs resubmitted.

## Next

1. Read the two jobs and verify; fill Evidence; mark ready for review.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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

- A1's `{"_year":..,"_month":..}` map: the pinned defect is the object time parts being unchecked (datetime-map-range); date parts were already checked by isValidDate. Fixed that.
- `_fsec` stays unchecked in the object form (lead named hour, minute, second; pin test_object_parts relies on integer `_fsec: 2`).
- Several zones in one map: the last wins.
