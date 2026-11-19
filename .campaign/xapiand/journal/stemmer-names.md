# stemmer-names: journal

State: waiting on jobs 261118-182339-5bd2c7 261118-182340-4a8ff4 261118-182336-f83449 261118-182338-4cee8e
Quest: .campaign/xapiand/quests/stemmer-names.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Review msg (correctness, CHANGES): F1 fixed at 18511f328: the three `get_data_field` reads (schema.cc) map the stored `_stem_language` through `_get_stemmer_name`, as feed and consistency do. Test `test_stored_alias_reads_its_stemmer` (tests/test_strict_text.cc): stored schemas naming nb, nn, NB, german2, German2, English read norwegian/german/english and `engine::parse_text` builds the stemmer. Jobs: F1 mutant (fix reverted, commit 343b10ab1 on local branch mut-stemmer-names-f1) 261118-182336-f83449 must fail; N2 mutant (the two removal commits reverted in schema.cc, final test, a6fb4191b on mut-stemmer-names-n2) 261118-182338-4cee8e must fail; suite build-test 261118-182339-5bd2c7; A2 parity rerun at the fix 261118-182340-4a8ff4. A2 at f0f313e64 (261118-115201-ed2420) passed earlier.

CI at f0f313e64: only "Unit tests under TSan" failed, on test_node (TSan data race on vptr, Exception::~Exception vs logger's default_describe_exception, WAL write_replace_document). Not this quest's files; the same failure is on master at 10d4be321 (CI run 44281033140). The other five CI jobs, FreeBSD and arm64 passed.

## Next

1. Fold the four job results into Evidence; then ready for review (re-review of F1).

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
| A2 | 261118-182340-4a8ff4 (at 18511f328; earlier 261118-115201-ed2420 passed at f0f313e64); F1 fails without: 261118-182336-f83449 |
| A3 | 261118-024858-f4b47c build-test (passed at 302d5ff53; only merges from master since); fails without: 261118-003721-65702e (aliases only), 261118-182338-4cee8e (final test, removals reverted: N2); build-test at the fix 261118-182339-5bd2c7 |

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
- CI TSan test_node failure is pre-existing on master (10d4be321); reported to the lead, not fixed here (outside the file set).
- N1 (stem_language_names a hand copy of the table) and N3 (stored removed name fails at first write or search) are nits; not changed. N3 is the owner's call per the review.
- msg for F1: answered above (fixed at 18511f328, test fails without the fix: job 261118-182336-f83449).
