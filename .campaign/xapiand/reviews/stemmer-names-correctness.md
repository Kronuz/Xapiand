# stemmer-names review: correctness

Commit: Xapiand:7d4df712a6964623cb86bc3b9307d0b87e0502e4
Verdict: CHANGES
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A2 261118-115201-ed2420 (at f0f313e64, same quest files as the head): 42 names served, each 0
  differ against Xapian 2.1.0 over its full snowball-data vocabulary (nb, nn -> norwegian 20,913
  equal; german2 -> german 35,053 equal); earlyenglish, english_lovins, english_porter refused. The
  oracle is called with the user's name and the parity tool with the stored stemmer, so it checks
  the alias mapping itself, word for word.
- build-test 261118-024858-f4b47c at 3ba079814: `git diff 3ba079814 7d4df712a` touches no quest
  file (only master merges in tests/test_engine_search_strata.cc, tests/test_loop_yield.cc).
- D118 (PLAN.md) for the per-name rulings.

## What I checked myself

Probe `.scratch/xapiand/review-stemmer-names-correctness/stored_probe.sh` (adds `stored_probe.cc`
to tests/test_strict_text.cc in the job checkout and runs it): job 261118-181347-0e9823, exit 0
(the first run, 261118-180318-eb86eb, aborted on an uncaught engine::Error in the probe itself).
It builds a stored schema whose `_stem_language` is a name as master stored it (raw), opens it,
reads it as a search does (`get_data_field` -> `engine::parse_text`), updates it, and runs index
term generation; and submits each removed name to a new schema.

Q1 (same stems, alias, or clear refusal). New schemas: nb/NB/nn -> stored `norwegian`, german2 /
German2 -> `german`, every other accepted name stored lowercased and stems equal to Xapian (A2).
earlyenglish, EarlyEnglish, english_lovins, english_porter -> ClientError (400) naming the 43
accepted names. `_language: nb|german2|earlyenglish` -> 400 (first=false names, unchanged). No
accepted name stems differently. One path breaks the alias: F1.

Q2 (removal complete). `git grep` over the tree (GPL paths excluded): the three names survive only
in tests/test_stem_parity.sh (listed so the row reports their refusal) and the test's removed
list. The phf table and switch no longer carry them (src/database/schema.cc:1639-1685). No doc
names any of the six or lists stem languages (text-type.md links only to Snowball); the field-name
language detection (schema.cc:2685, 6864, 6896) only takes first=true names, which never included
these. The query DSL takes stem_language only from the stored schema (src/query_dsl.cc:839-840).

Q3 (existing indexes naming a removed stemmer). Never silent: open succeeds; an update naming the
same name passes the schema check, then term generation fails with engine::Error invalid (400)
"Language code earlyenglish unknown" (src/multivalue/generate_terms.cc:510-515); a search fails
with InvalidTextConfigError (400) "Language code earlyenglish unknown" (src/engine/text.cc:297-300).
Changing to another name is refused ("not allowed to change"). Same behavior as master: snowball
never knew these names, so no strata-era index holds stems for them, and an alpha.9 index arrives
through `_restore`, whose settings go through `process_stem_language` and get the 400.

Q4 (values, mutant). The alias test checks stored names and real stems (aabakken -> aabakk,
aalglatten -> aalglatt); the refusal test checks each listed name is accepted and builds a
snowball stemmer, and each removed name is refused and unlisted. `CHECK_GT(names.size(), 40u)` is
a count, backed by those value checks. Mutant 261118-003721-65702e (5a49687e4: aliases, no
removals) fails on 3 checks, `!snowball::stemmer::canonical(stemmer).empty()` for the three names
the schema then stored unbuildable: meaningful for A3. See N2 for what it did not exercise.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The alias holds on the write path but not the read path. A schema stored before this branch with `_stem_language` `nb`, `nn`, `german2` (or a capitalized name such as `NB`, `English`; master stored the name raw) now indexes with norwegian/german/english stems, because `feed_stem_language` maps it (schema.cc:7108), while every search on the field reads the raw stored value (schema.cc:9565, 9620, 9720) and fails: "Language code nb unknown". Before the branch both paths failed; now documents go in that no search can reach. | probe 261118-181347-0e9823: stored=nb/nn/german2/NB/English: open ok, update ok, `get_data_field` -> raw name, parse_text -> ClientError "Language code nb unknown" | Map the read with the same `_get_stemmer_name` (one normalization, used by feed, consistency and the three `get_data_field` reads). Test in test_strict_text.cc: a stored schema naming `nb` (and `German2`) gives `get_data_field("body").first.stem_language == "norwegian"` (`"german"`) and `engine::parse_text` builds its stemmer. |
| N1 | no | nit | `stem_language_names` (schema.cc:1629) is a hand copy of the phf table (schema.cc:1639); the test checks listed ⊆ accepted, not the converse, and A2's accepted.txt comes from the list, so a name added only to the table would escape both. They match today (43 names). | schema.cc:1629-1633 vs 1639-1684 | Derive the list from the table, or check both directions in the test. |
| N2 | no | gap | The mutant ran the test as it was at 5a49687e4, with an empty removed list; the final test's removed-name half (refused, unlisted) never ran against a mutant. It fails by inspection without the removals (the names would be accepted). | job 261118-003721-65702e spec; `git diff 5a49687e4 7d4df712a -- tests/test_strict_text.cc` | Optional: rerun the final test against the removals reverted. |
| N3 | no | nit | A stored schema naming a removed stemmer opens without complaint and fails only at the first write or search, with "Language code earlyenglish unknown" rather than the accepted-languages message. Clear and 400, never silent. Refusing on open would make such an index unopenable: the owner's call (keep as is, or reword the term-generation and search errors to say the name was removed). | probe 261118-181347-0e9823, stored=earlyenglish/english_lovins/english_porter | Owner's call; keeping it is fine. |

## Not run

- No end-to-end HTTP run against a node; schema and analysis paths were driven in-process.
- No check of `_restore` with an alpha.9 dump naming these names; inferred from
  `restore_documents` routing settings through the schema update (src/database/handler.cc:1146).
- CI at 7d4df712a (running at review time) and its TSan test_node failure (journal: pre-existing
  on master) are outside this lens.
- No GPL source read (src/xapian, sortable_serialise, sortable.py).
