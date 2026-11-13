# exact-id-keys: Numeric ids are exact keys up to the integer range

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status exact-id-keys`.

## Outcome

Every numeric `_id` is its own key: a document's id term, its routing and every lookup by id encode the number exactly with Kronuz/sortable (MIT, exact for every int64 and uint64), so ids above 2^32 no longer collide (PLAN P2: on master PUT /p/4294967297 then PUT /p/4311744512 leaves one document). one-id's automatic-id cap (`MAX_NUMERIC_ID` = 2^32-1) rises to the largest id the codec keeps exact. Only id terms change: value slots, sorting and ranges stay on the old encoding until sortable-encoding (owner Q1-Q3 still open). Owner D86: fix now rather than reject large ids.

## Acceptance criteria

- A1 Ids that collide today stay distinct: 4294967297 and 4311744512, 2^40+3 and 2^40+3*2^24, 2^53-1 and 2^53+1, 2^63 and 2^64-1 are separate documents through PUT, GET, DELETE, RESTORE and a term search on `_id`, across a reopen: a test with values, and a `--fails-without` job putting the old id encoding back
- A2 Automatic ids pass 2^32 without failing: a counter started near 2^32-1 issues the next ids above it, they keep the bucket stride (D85), and the cap is the codec's exact bound: a test with values and a mutant restoring the 2^32-1 cap
- A3 [artifact] The note names every place an id term is built or parsed and its encoding, and the GPL code is not read or used for ids: `docs/design/exact-id-keys.md`
- A4 The suite passes, and CI's sanitizers are green at the landed code

## Contracts

Changes the on-disk id terms (pre-release format change, formats open until release, D78/one-id precedent: no migration; earlier indexes need a dump and RESTORE). Provides `xapiand:id-terms`: a numeric id's term is a fixed prefix plus `sortable::encode(N)`; string ids are unchanged. Consumes Kronuz/sortable (pin it in CMakeLists.txt as the other Kronuz libraries are).

## Decisions

- Owner D86 (2026-11-13): fix the collision now ("fix fast now, unless a quick fix is too difficult"); ids only, values stay with sortable-encoding.
- Lead: never read the GPL encoder (src/sortable_serialise.*); replace its use for id terms by calling Kronuz/sortable, judged by behavior tests.

## May land early

None.
