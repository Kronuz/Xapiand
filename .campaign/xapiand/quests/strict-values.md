# strict-values: One strict rule for values, ids and searches

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strict-values`.

## Outcome

Per D92, a value must be exactly a value of the field's type wherever Xapiand reads one: field writes, `_id` writes and searches (term, range bounds, `q=`). A mismatch is a 400 naming the field, the value and the type: `1.5` or `"abc"` in an integer field, `-1` in a positive one, `2^64` in an integer one. Nothing is truncated, wrapped or silently turned into a no-hit query. An explicit cast object (`{"_integer": 1.5}`) still converts one value on request, the same way on writes and searches. Text that spells a number exactly is accepted (`"12"` in an integer field), as alpha.9 did.

## Acceptance criteria

- A1 Writes: `PUT` with `_id` 1.5 on an integer `_id`, -1 on a positive `_id`, and "abc" on either answer 400; the same values in an ordinary integer or positive field answer 400; exact values (including "12" as text, 2^63-1, 2^64-1 on positive) still store and read back: unit tests, plus E2E blocks in the docs page
- A2 Searches: term, range bound and `q=` searches with those mismatched values answer 400 on `_id` and on ordinary fields (master and alpha.9 answered 200 with no hits; the exact-id-keys review probe 261115-215450-71a3de covers the `_id` cases); exact values still match: unit tests and E2E blocks
- A3 Casts: `{"_integer": 1.5}` stores and finds 1; `{"_positive": -1}` is still a 400 (a cast converts types, it does not wrap): tests
- A4 Every error names the field, the value and the expected type: tests check the message
- A5 Nothing else changes: the full suite and the docs E2E pass, with only the blocks this quest changes edited: the `build-test` and `e2e` checks
- A6 [artifact] Changelog `[Unreleased]` states the stricter behavior and the cast escape: `docs/src/content/docs/changelog.md`

## Decisions

- Owner, 11-16 (D92): strict everywhere with a 400, explicit per-value casts, no `_coerce` mode.
- Lead: the global-query fallback on a SerializationError in query_dsl.cc goes; a typed field with a mismatched value is the client's error.

## May land early

None.
