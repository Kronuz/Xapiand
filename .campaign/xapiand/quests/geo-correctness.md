# geo-correctness: Geospatial answers are right for wide circles, lenses, rings and bad WKT

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status geo-correctness`.

## Outcome

coverage-areas found five wrong geospatial answers (tests/test_coverage_geospatial.cc, findings F18-F22, pinned there as today's behavior): malformed `MULTIPOINT(1 2, 3)` raises a non-EWKTError (500, not 400) (F18); a collection holding an empty intersection does not write EMPTY (F19); the lens of two 9,000 km circles 45 degrees either side of a center does not cover it (F20); a 15,011 km circle (past a hemisphere) does not cover its own center (F21); a point 5,500 km from the antipode inside both circles of a ring is not covered (F22). Each is fixed, or ruled correct with the geometry that shows it.

## Acceptance criteria

- A1 Each of F18-F22: a test asserting the correct answer, failing at the parent commit (`--fails-without`), or a [decision] row showing the current answer is right
- A2 Geo queries over a corpus give the same hits before and after except where F18-F22 change them, listed: a job with values
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: wrong search answers are correctness bugs; tests that pin them stay only until this quest replaces them.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>


F20 is an htm bug (D129): it is fixed in quest htm-edge-points, and this quest repins htm to that landed commit (CMakeLists.txt joins the file set then) and flips the F20 pins.

D130: also serve `_multiconvex` in the map form (src/geospatial/geospatial.cc), the one form where it fails; a test failing at the parent commit.
