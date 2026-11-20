# datetime-range-zone review: correctness

Commit: Xapiand:1c97fba6fe04da5f0e9e36b29d204c0d6fcea2d1
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261119-163517-332cfd (test_coverage_core_datetime passed), 261119-163518-7d7ef0 (the same test fails
  with master's src/datetime_msgpack.cc), 261119-163519-1b1b09 (suite passed). All ran at
  18395fff1; the merge to 1c97fba6fe04 changes none of src/datetime_msgpack.*,
  tests/test_coverage_core_datetime.cc, or the callers (serialize.cc, schema.cc, query_dsl.cc,
  multivalue/range.cc, CMakeLists.txt), so they still hold.

## What I checked myself

Scripts are in `.scratch/xapiand/review-datetime-range-zone-correctness/`: `probe.cc` and `run.sh [old]`.
The probe builds in place of the coverage test and prints its observations.

- 261119-174401-7ccf37 (quest code, at 1c97fba6fe04) and 261119-174403-78221e (the same probe on
  master's datetime_msgpack.cc from ca713b622).
- Q1, instants: I took 10 local datetimes and tried each at +02:00, -05:00, +05:30 and -03:30. They
  cover no carry, day back and forward, month forward, Feb 29 and Feb 28 back, year forward and
  back, a fraction, below year 1 and past 9999. Each one went through the string path and 7 map
  forms: `_date`/`_time` in both orders, flat `_year/_month/_day` in both orders, `_datetime` nested
  in both orders, and a JSON-parsed map.
  - The quest code has 0 instant disagreements in 280 comparisons. Master has 72, all in forms where
    `_time` comes before the date.
  - The year-1 `+` cases throw "Year is out of range" on both paths.
- Q1, errors: the map now refuses 2021-02-29 with a zone, as the string path does. Master's map
  moved it to 2021-03-01. `_min` 60 and `_sec` 60 are refused, and year 0 is refused. The
  messages differ in wording but each one names the part that is wrong.
- Q2, values: I fed `_hour`, `_min`, `_minute`, `_sec` and `_second` these values: uint and int 0,
  limit-1 and limit; -1; INT64_MIN; UINT64_MAX; 2^32 and 2^32+5 (the int-truncation trap); floats
  1.0, 1.5, -0.0 and NaN; a bool; a string; nil; JSON limit-1, limit, -1 and 1.0. I tried each at
  the top level, in `_time` and in `_datetime`.
  - Every out-of-range value is refused, and every in-range value is accepted, including signed
    JSON integers.
  - The three forms agree everywhere.
- Q3, `_fsec`: 11 cases. Results are in F1.
- Q4, callers: the changed functions are file-static. The only external entry point is
  `Datetime::DatetimeParser(const MsgPack&)`. Its callers are serialize.cc:506-515 and 664-674,
  schema.cc:5788-5794 (stored text), query_dsl.cc:498 and 1485, and multivalue/range.cc:186-187.
  - Each one sees the same shift, and only for maps: out-of-range hour/min/sec now throw, a `_time`
    zone lands on the final date, and an invalid date with a zone is refused.
  - The exception type is still DatetimeError, so callers handle it as before.
- Coverage: the new range checks and Zone lines (datetime_msgpack.cc:47-157) are all hit by the
  probe run.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | **`_fsec` is still unchecked.** This existed before the quest and was ruled out of scope, but a map can still name an instant that no other path produces, or store text that differs from the instant it indexes. Year 1 with `_fsec: -1` indexes 0000-12-31T23:59:59, which the number path refuses with "Year is out of range". NaN and ±inf give a non-finite timestamp. `_fsec: 2` stores `10:00:59` but indexes `10:01:01`. `_fsec: 86400` stores `10:00:006400`. `_fsec: 0.9999999` stores `:00` but indexes `:01`. coverage-core-small listed fsec in the same pinned defect (`datetime-map-range`), and the old pin used `_fsec: 5.0`. Refusing any of these narrows accepted input, so this is the owner's call. | 261119-174401-7ccf37 (Q3 section). The same output on master is in 261119-174403-78221e. Code at datetime_msgpack.cc:162-176. The origin is master's .campaign/xapiand/journal/coverage-core-small.md:51. | Options: (a) refuse non-finite values and anything outside [0, 1); test_object_parts' `_fsec: 2` pin becomes a value in [0, 1). This is my recommendation. (b) Clamp with `normalize_fsec` as the string path does; this silently turns 2 into 0.999999, which I don't recommend. (c) Refuse only non-finite and negative values. (d) Keep it and file a hunt row. Test for (a): `_fsec` -1, 1, NaN and inf throw; 0.999999 is accepted. |
| F2 | no | defect | **A map with a zone stores its text without `Z`.** The string `2020-01-02T10:00:00+02:00` stores `2020-01-02T08:00:00Z`, but the same datetime as a map stores `2020-01-02T08:00:00`. The instant is the same, and the text parses back to it. A2 says "stored", and the map path never sets `is_utc`. This existed before the quest. | 261119-174401-7ccf37: all 266 map cases that agree on the instant show `utc=0`. Code at datetime_msgpack.cc:52-56; the stored value is written at schema.cc:5788. | Set `tm.is_utc = true;` in `Zone::apply`. Test: `SEE(Datetime::iso8601(map form), Datetime::iso8601(string form))`. |
| F3 | no | nit | **A zone outlives a later `_time` that has no zone.** `{"_time": "10:00:00+02:00", "_datetime": {"_time": "11:00:00"}}` gives 09:00; master gave 11:00. The ruling is that the last zone wins, so a later `_time` should own its zone, including having none. The input is an ambiguous duplicate. | 261119-174401-7ccf37 (Q1 errors: "zone then zone-less _time") | Reset `zone = Zone{}` at the start of `process_date_time(tm, zone, std::string_view)`, or keep the current behavior. |
| F4 | no | nit | **The wrong error message for hour 24 with a zone.** `_time: "24:00:00+02:00"` reports "Error format in _time" instead of "out of range": the default branch falls through to `error` when hour >= 24. This existed before the quest. | 261119-174401-7ccf37 (Q1 errors). Code at datetime_msgpack.cc:223-298. | Jump to `goto error_out_of_range` when a field is out of range in the default branch. |
| F5 | no | defect | **`_year`, `_month` and `_day` silently truncate.** A u64 value is cut down to `int`: `_year: 2^32+2020` parses as 2020, and `_month` or `_day` of 2^32+1 parses as 1. The string path cannot produce this. It existed before the quest and is outside its change. | 261119-174401-7ccf37 (Q2 date parts). Code at datetime_msgpack.cc:60-99. | File a hunt row: check against the int range before assigning. |
| F6 | no | nit | **A1's wording does not match its source.** The quest says a map range "selects exactly its span", but A1 came from the pin `datetime-map-range`, which is about unchecked time parts. The contributor's ruling follows the pin, so there is nothing to fix; the lead may want to reword A1. | Master's .campaign/xapiand/journal/coverage-core-small.md:51; the journal's "Rulings I made" | None in code. |
| F7 | no | nit | **One caller loses an input.** Casting `{"_datetime": {"_hour": 30}}` into a timedelta field used to give 30 h and is now refused. The cast word names a datetime, so the refusal is consistent, but this narrows accepted input and is the owner's call. I found this by reading the code; I did not probe it. | serialize.cc:503-513, which calls DatetimeParser on the map | Keep the refusal (recommended), or accept the old behavior. |

## Not run

- I did not run indexing, storing or querying end to end through the HTTP server. Instant equality
  is shown at `DatetimeParser`, which all three call; the stored text is shown through
  `Datetime::iso8601`.
- I did not probe accuracy-term queries (query_dsl.cc:498) or sorting with a zoned map.
- No sanitizers, no Linux, no FreeBSD; those are CI's, and CI is still running at 1c97fba6fe04.
