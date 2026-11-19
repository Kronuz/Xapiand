# serialize-validation: Negative text never becomes a positive number; unknown type names are refused

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status serialize-validation`.

## Outcome

coverage-areas' serialize tests (tests/test_coverage_serialize.cc, Xapiand:306690573) found two silent acceptances: a negative text such as "-1" serializes as a positive number (strtoull wraps it to U64_MAX), and `get_field_type` maps a word that is no type name to a real type (its perfect hash has no membership check). Both are refused with a 400 naming the value. Three more from the same tests (Xapiand:439d0ee24): every geo point serializes to the same value bytes, because `std::hash<range_t>` XORs a range's start with its end and a point's range has start == end (`Serialize::ranges_hash`, src/serialize.cc:927); `get_field_type("boolean")` returns datetime; and `guess_serialize` sends string `_time` and `_timedelta` values through datetime, which throws. The `_date`, `_multiconvex` and `_lua` casts are refused; each is either served or removed by the owner's ruling.

## Acceptance criteria

- A1 "-1", "-0x1" and " -1" for a positive field are refused; "0" and U64_MAX still serialize: a test failing at the parent commit (`--fails-without`)
- A2 Every non-type word tried (the coverage test's list plus 10,000 random words) is refused, and every real type name still maps to its type: a test failing at the parent commit
- A3 Distinct geo points serialize to distinct values, and the stored value's users (sort, exact match, aggregations) are listed with what the change does to existing indexes: a test failing at the parent commit, and a note in docs/
- A4 `get_field_type` maps "boolean" to boolean, and string `_time` and `_timedelta` values serialize as time and timedelta: tests failing at the parent commit
- A5 [decision] `_date`, `_multiconvex` and `_lua` casts: served, or removed by the owner (a Decisions row)
- A6 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: refusing is a bug fix, not a narrowing: these inputs were silently wrong, never served correctly.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
