# serialize-validation: Negative text never becomes a positive number; unknown type names are refused

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status serialize-validation`.

## Outcome

coverage-areas' serialize tests (tests/test_coverage_serialize.cc, Xapiand:306690573) found two silent acceptances: a negative text such as "-1" serializes as a positive number (strtoull wraps it to U64_MAX), and `get_field_type` maps a word that is no type name to a real type (its perfect hash has no membership check). Both are refused with a 400 naming the value.

## Acceptance criteria

- A1 "-1", "-0x1" and " -1" for a positive field are refused; "0" and U64_MAX still serialize: a test failing at the parent commit (`--fails-without`)
- A2 Every non-type word tried (the coverage test's list plus 10,000 random words) is refused, and every real type name still maps to its type: a test failing at the parent commit
- A3 The suite passes and CI is green

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

- Lead 2026-11-18: refusing is a bug fix, not a narrowing: these inputs were silently wrong, never served correctly.

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
