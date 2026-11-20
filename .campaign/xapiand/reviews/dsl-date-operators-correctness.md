# dsl-date-operators review: correctness

Commit: Xapiand:4ec7371e5d690d3da06975f255646343b8a38a9e
Verdict: CHANGES
Date: 2026-11-19

## Evidence I relied on

- 261119-225927-d1b679 (the quest's HTTP test passed at 6aa50da6c) and 261119-230242-705e99 (the same test, passed at 4ec7371e5): A1 and A2 values for field and global queries. The code since the first run is the master merge and docs.
- 261119-225934-6b9166 (the test exits 1 with master's query_dsl.cc): the fix is what makes it pass. Its run at the head (261119-230250-39622d) and the suite (261119-230243-24022b) were still queued or running when I finished; A3 and CI are not judged here.

## What I checked myself

Probe scripts under `.scratch/xapiand/review-dsl-date-operators-correctness/`, run as jobs at 4ec7371e5 (the node binary is built in the quest's slot):

- 261119-230541-b1f5db (`probe.py`): about 100 queries through `_date`, `_time`, `_timedelta` against a field, a missing field, leaf paths, accuracy paths and the global query; zone, fraction and negative values; every type crossing.
- 261119-230849-ac4e46 (`probe2.py`): global docs on distinct fields, one per type, with equal seconds in a time and a timedelta; bogus maps against the `_datetime` control.
- 261119-230945-7e797b (`probe3.py`): the raw reply of `_date` on a missing field and on an integer field.

Answers to the lead's questions:

1. Exactness. On a typed field the bridge sends `_date` as `_datetime` into the same `Serialize::object` path that `_datetime` takes, and indexing of date and datetime is one code path (`Serialize::datetime(value, tm)`), so the terms equal what was stored. Probed equal to the typed spelling: a date, a date as ISO text, as a `_year/_month/_day` map and as a timestamp; a datetime with a zone (`+05:00` equals the stored 05:30Z, and a `Z` spelling of the same instant); a time with fractions (`10:30:00.250` matches, `10:30:00` does not); a negative timedelta as text and as seconds; seconds numbers for time and timedelta. `_time` and `_timedelta` take the same `time_to_double`/`timestamp` conversion as the indexer (`Serialize::time(value, t_val)`), for fields and for the global bridge. Not matched: accuracy paths and `_in` bounds (C4, C5), which refuse with 400 as `_datetime` does.
2. Global typing. `_time` and `_timedelta` do take their own type in `get_global_query`, but time and timedelta share the term prefix (`get_ctype` is `'T'` for both) and the same bytes for equal seconds, so they match each other: C1. `_date` and `_datetime` match each other by design (midnight). No other crossing: a global `_datetime` at 1970-01-01T10:30:00 is not found by `_time 10:30:00`.
3. Docs. The reply "200 with no hits for `_date` on a missing field" is true: `{"joined": {"_date": "2020-01-01"}}` answers `200 {"hits":[],"count":0,...}` (261119-230945-7e797b); the bank dataset has no `joined` field and no dates. The 400 texts `'_date' must be a map value` and `Error format in _datetime` are real (test, probe). The sentence that these find "the documents whose field holds that time delta, or with no field at all" is not exact for the global case (C1).
4. Newly accepted input (was 400 "Invalid operator"): all hit documents equal to the typed spelling, except C3, C6 and C7 below. A cast value of the wrong type for a field still answers 400 (`_time` on a date field, `_date` on a time field, `_timedelta` on a datetime field, `_date` on an integer field).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | defect | A global `_time` finds documents indexed with an equal-seconds timedelta, and a global `_timedelta` finds times: with a doc time `10:30:00` and a doc timedelta `+10:30:00`, both `{"_time": "10:30:00"}` and `{"_timedelta": "+10:30:00"}` return both documents (so do the numbers 37800). Cause: time and timedelta use one global term prefix (`'T'`, `src/database/schema.h:get_ctype`), so the query's type cannot separate them; it is the same for the plain text guess `"10:30:00"`. The docs and the code comment say each operator names its type. The quest's test has no equal-seconds pair, so it cannot see it | 261119-230849-ac4e46 (`global _time 10:30:00` -> a, b; `global _timedelta +10:30:00` -> a, b; `global time text guess` -> a, b) | Keep the operators. State in casting-values.md that a global `_time` and `_timedelta` share one term space (equal seconds match across the two; negative timedeltas and values over 24h never match a time), and add a test with a time and a timedelta of equal seconds that pins the documented result. Separating the prefixes is an indexer change (`src/database/schema.cc`, a format change) and an owner decision, outside this quest. Test the fix must add: in `tests/test_query_dsl_date_http.py`, the `gl` docs with `10:30:00` and `+10:30:00` and the exact hit set for both global queries |
| C2 | no | gap | The test misses most of what it claims for exactness: no zone, fraction, negative timedelta, number value, a `_date` with a time of day, or the equal-seconds pair. All pass today by probe | 261119-230541-b1f5db | Add them as cases to `tests/test_query_dsl_date_http.py` |
| C3 | no | defect | A map the date parser does not know answers 500 (`{"d": {"_date": {"_bogus": 1}}}`, global too: "Unsupported Datetime: '_bogus'"). `_datetime` answers the same 500 today, but `_date` was a 400 before, so it is a client error newly reaching a 500. The throw is in `src/datetime_msgpack.cc` (not a `DatetimeError`), outside this quest's files | 261119-230849-ac4e46 (`d _date bogus map` 500, `d _datetime bogus map control` 500) | Hunt row: make the parser throw `DatetimeError`; no change here |
| C4 | no | gap | On an accuracy path (`dt._day`, `t._thour`, `td._tdhour`) a cast operator, `_date`, `_time`, `_timedelta` and the existing `_datetime`, answers 400 with a misleading text (`Error format in _time: 2020-01-02T10:30:00...`, `Time must be numeric or string`); the plain value works. Refused, never a wrong hit | 261119-230541-b1f5db (accuracy section) | Hunt row, same cause for `_datetime`; or keep |
| C5 | no | gap | `_in` bounds cast with `_date`, `_time`, `_timedelta` answer 400 (misleading texts: `'_date' must be a map value`, `Time must be numeric or string`); text bounds work. `_datetime` bounds also 400 | 261119-230541-b1f5db (`_in` section) | Hunt row, same cause for `_datetime` |
| C6 | no | nit | `_date` is only the `_datetime` spelling: a `_date` with a time of day is accepted and not truncated (`{"d": {"_date": "2020-01-02T10:30:00"}}` finds nothing on a date field, finds the datetime on a datetime field). A bare `_date` on a datetime field at 10:30 finds nothing. The docs say it is cast as `_datetime`, which states it | 261119-230541-b1f5db | Keep; or say in the docs that `_date` is the instant at midnight |
| C7 | no | nit | `_date` does not pass through a leaf `_value` or a leaf `_or` map: `{"d": {"_value": {"_date": ...}}}` answers 400 "Unknown cast type '_date'", while `_time`, `_timedelta` and `_datetime` work there (`Serialize::object` has no `_date`, outside the file set). A `_date` on an integer field reports `got {"_datetime": ...}` | 261119-230541-b1f5db (leaf section) | Hunt row for `serialize.cc`; no change here |
| C8 | no | nit | An out-of-range `_time` (90000) on a time field answers 200 with no hits, on the global query 400; the plain number 90000 on the field does the same. The docs Type Compatibility table does not list the three new words | 261119-230541-b1f5db | Hunt row; docs table if wanted |

## Not run

- The mutant at the head (261119-230250-39622d), the suite (261119-230243-24022b) and CI: queued or running when I finished. I did not run a mutant that reverts only the global bridge, but without it `guess_serialize` reads a `_time` map through `datetime(Cast::time(...))` (`src/serialize.cc`), which fails on "10:30:00", so the quest's global `_time` case fails without it.
- Sort, aggregation and `_in` global ranges with these words; other quests' code paths.
- Other platforms.
