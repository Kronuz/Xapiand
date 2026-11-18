# stemmer-names: journal

State: waiting on jobs 261118-024858-f4b47c 261118-024902-cbb6ac 261118-024903-704f38
Quest: .campaign/xapiand/quests/stemmer-names.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Merged master (365fd3b57). Interrupted jobs 261118-024902-cbb6ac and 261118-024903-704f38 lost their runner before starting (infrastructure, not a test failure); A2 resubmitted at the merged head. A3 mutant 261118-003721-65702e (done) and build-test 261118-024858-f4b47c (done) stand; the vocabulary row earlyenglish exit 1 is the expected difference (2,849 differ), which is why it was removed per D118.

## Next

1. Fold job results into the table below; then ready for review.

## Blocked on

Nothing. A1 ruled: PLAN.md D118 (nb, nn, german2 aliases; earlyenglish, english_lovins, english_porter
removed), matching the branch's removals.

## Evidence

Vocabulary jobs (Xapian 2.1.0 via strata-oracle's `oracle stem` vs snowball via prospect's
stem_parity 2cc69d4d1e91, snowball-data vocabularies as release-rows A4 used):

| Name | Compared with | Result | Job |
|---|---|---|---|
| nb | norwegian | 20,913 of 20,913 equal | 261118-002155-22aee4 |
| nn | norwegian | 20,913 of 20,913 equal | 261118-002203-9c6e13 |
| german2 | german | 35,053 of 35,053 equal | 261118-002212-d6a754 |
| earlyenglish | english | 2,849 of 36,534 differ | 261118-002220-8a7a7f |
| english_lovins, english_porter | - | Xapian refuses both (release-rows 261111-201059-749f46) | - |

| Criterion | Evidence |
|---|---|
| A1 | D118 (PLAN.md Decisions) |
| A2 | 261118-024902-cbb6ac (queued, head 3ba079814; earlier 261118-003712-6c88e6 at pre-rebase) |
| A3 | 261118-024858-f4b47c build-test, 261118-024903-704f38 mutant at 95104c4b2 (head's tests, pre-removal schema; scratch branch mut-stemmer-names) (queued) |

## Result

- `src/database/schema.cc`: `_get_stemmer_name` stores nb/nn as norwegian and german2 as german
  (process, feed and consistency); refusal lists the accepted names; earlyenglish,
  english_lovins, english_porter removed.
- `tests/test_strict_text.cc` (build-test): alias, refusal and removed-name cases; prints the
  stored stemmer per accepted name for `tests/test_stem_parity.sh` (A2: release-rows' stems row
  through the stored stemmer names).

## Rulings I made

- Stored stem_language is the lowercased stemmer name (nb -> norwegian); a stored schema naming
  nb feeds as norwegian, so resubmitting nb passes the consistency check.
- Per msg from the lead: CMakeLists.txt untouched (hunt-lua-fetch holds it); the cases live in
  tests/test_strict_text.cc, the registered schema test, which the lead named. Branch restacked
  from a531eabd6 so each removal stays its own commit; old jobs 261118-003117-68b230,
  261118-003120-4a78a1, 261118-003125-1bdbe0 cancelled.
- Docs: no doc names any of the six; no docs change.
