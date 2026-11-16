# wbuf-inline-prospect: prospect repins strata with the inline write buffer

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status wbuf-inline-prospect`.

## Outcome

prospect pins strata's main with `wbuf-inline` landed, and its budgeted flush test writes enough
records for the flush points that real-stride charging moves (Kronuz/prospect; strata
`docs/status/wbuf-inline.md`, Done 6; review NEW-1).

## Acceptance criteria

- A1 `test/index_test.cc`'s budget case writes 16,000 records and keeps more than two segments for `index` and `index_mapped` (wbuf-inline ledger Done 6; review NEW-1): the `build-test` check
- A2 prospect pins strata at the main commit where `wbuf-inline` landed (wbuf-inline ledger Next 1): the `build-test` check at the landed `CMakeLists.txt`
- A3 prospect's suite passes (51 of 51 at strata:c439e69) and its churn and dump are identical to prospect on the earlier strata at flush threads 1 and 2 (wbuf-inline ledger, chunk round): the `build-test` check and a churn job

## Contracts

Consumes strata main after `wbuf-inline`.

## Decisions

- Adopts `test/wbuf-inline-flushes` (prospect:c0c4260); the repin commit is added on it.
- `--after wbuf-inline`: landing is ordered, never atomic; prospect's main keeps its old pin until then.

## May land early

None.
