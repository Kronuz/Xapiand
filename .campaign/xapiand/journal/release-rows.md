# release-rows: journal

State: ready for review
Quest: .campaign/xapiand/quests/release-rows.md (the lead's; never edit it)

## Doing

Done. Head `prospect:0ba797e` (report only, pushed). Correctness review CHANGES (F1-F8) folded in
as text, no code change: A3 and A5 recorded as misses per the lead's rulings; F3 gaps, F4 probe,
F6 label in the report; F5, F7, F8 under its Residuals. Every row ran at
`prospect:2cc69d4` at Xapiand's pins; the report is `docs/gates/release-rows.md`.

Answered: the lead's message of 11-11 (jobs ended; A4 a MISS with six names; fuzz; report; push;
ready for review): all done as asked. A4 is recorded as a miss with each name, a sample, and
Xapiand's mapping (`_get_stem_language`: earlyenglish, english_lovins, english_porter -> en;
german2 -> de; nb, nn -> no; each `false`, not a language name; the stemmer is built from the
name as given, so the six are refused at analysis with "Language code NAME unknown"). Not fixed
here (hunt row "Six stemmer names").

## Next

Nothing: the correctness review.

## Inventory (prospect base `0aa68f4`)

The release's pins (Xapiand master `8ddcd247f`, `CMakeLists.txt:771-772`): prospect `850d4e8`,
strata `5d742b2` (Xapiand declares strata first, so its pin wins over prospect's own `9d52824`).
Xapiand also overrides three of strata's libraries: compressors `5566260` (strata pins `5c54128`),
hashes `03ab2d6` (`e3bf5b6`), threadpool `63e03a5` (`eb4751a`); unicode, snowball, analysis, task
equal prospect's pins; docset, bitpack are prospect's.

- prospect `include/` is identical at `850d4e8` and `0aa68f4` (`git diff --stat` empty): the
  engine is the pin's; only the strata pin differs (main `94480d5`, the manifest log, is past the
  release's `5d742b2`). Six main tests use the manifest log (background_merge, fixtures, format13,
  media, index, reclaim) and do not build at `5d742b2`; the rows' targets do not use it.
- The release's strata lacks the stored-frame cache (strata `71750940`) and the frame-overlap
  rejection (`94480d52`) that `fuzz/MEMORY.md` records as fuzz_postings' RSS fix.

| Crit | Harness that exists | Missing | Jobs |
|---|---|---|---|
| A1 | `tools/parity.cc` (1/N segments, `PARITY_DELETES`, merge after deletes); `tools/prospect_engine.cc:430` verify over an index, `:819` merge; phase 3 ran both on the gate VM (`bench/gate3.sh:233`, `tools/vm/gate_job.sh`) | probe checks (pruned = counted, live, unique) in the tool; a host run at the release pins | parity 1/4/16 + deletes 4/16 (10k of the corpus, rank.jsonl); verify on a 1M multi-segment index before and after merge |
| A2 | `test/operators_test.cc` (ops.jsonl: AND, OR, XOR, MAX, SYNONYM, FILTER, AND_MAYBE, PHRASE, SCALE_WEIGHT, ELITE_SET), `tools/parity.cc` (rank.jsonl: AND, OR, AND_NOT, AND_MAYBE, PHRASE, NEAR), `test/shards_test.cc` (shards-s*.jsonl) | per-operator tally in the report; WILDCARD and EDIT_DISTANCE are Xapiand expansions into OR/SYNONYM/MAX; `source` is Xapiand's | operators_test, shards_test, parity at the release pins |
| A3 | `test/aggregate_test.cc:268` (in-memory partials, 1-64 splits); `test/shards_test.cc` section 4 (the visitor sees every live match once) | indexed one shard vs many over values; Xapiand master serves its own `src/aggregations` (long double sums, spy order = shard order), not prospect's | new shards_aggregate_test |
| A4 | snowball's `test/vectors` (500 words/language); the oracle's `stem LANGUAGE WORDS` (`strata-oracle/vectors/.scratch/oracle`, Xapian 2.1.0) | every name Xapiand's schema accepts (`src/database/schema.cc:1629`), full vocabularies | stem_parity per name |
| A5 | `fuzz/fuzz_{segment,postings,values,fielddict}.cc` + corpora; `test/format13_test.cc:290` version +-1 (main only: manifest log) | a version test that builds at the release pins, per fixture | 4 decoders x 6 x 10 min; versions_test |
| A6 | CI `asan` on `quest/**` pushes | | push the branch |
| A7 | | `docs/gates/release-rows.md` | |

## Blocked on

Nothing. For the lead (not blocking): `fuzz_postings` peaked 1,586-2,045 MB of its 2,048 MB limit
at the release's `strata:5d742b2` (the ASan-quarantine growth `fuzz/MEMORY.md` records, fixed at
`strata:71750940`); zero findings, but a longer run at this pin stops on memory. Moving the
release's strata pin past `71750940` removes it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261111-201053-f77c3a (parity 1/4/16 segments, deletes 4/16 then merged: 419/419 counts each, exact after merges), 261111-201056-33db85 (verify 1M, 8 segments, 90,910 deletes, then merged: 0 mismatches over 962 + 200 queries both passes); passed at 2cc69d4 |
| A2 | 261111-201050-c7d247 (operators_test per operator, shards_test 3,696 oracle rows), 261111-201053-f77c3a (parity by operator: AND, OR, AND_NOT, AND_MAYBE, NEAR, PHRASE all equal), 261111-201230-8ef3e1 (pin's 51-test suite at strata 5d742b2); passed at 2cc69d4 |
| A3 | MISS at the release's level (lead's ruling on F1): 261111-201050-c7d247 proves prospect's collectors only (240 comparisons, 12,006 matches, 1,721 deletes); the release's row is hunt row 91 |
| A4 | [artifact] `docs/gates/release-rows.md`: "**miss**: six names" (equality is quest stemmer-names) |
| A5 | MISS as one CPU-hour (lead's ruling on F2; six independent 10-min wall-clock starts per decoder, coverage still rising; hunt row 92): 261111-201102-4af80d (fuzz-build ASan+UBSan), 24 fuzz runs 261111-201105-74a86d 261111-201108-63232e 261111-201111-fb8de3 261111-201114-9f665c 261111-201117-334338 261111-201120-b8edb5 261111-201123-48fad1 261111-201126-e1d239 261111-201129-9bc82b 261111-201132-4a9573 261111-201135-50a022 261111-201138-698b31 261111-201141-365038 261111-201144-b67b2c 261111-201147-634ad4 261111-201150-c36d62 261111-201153-bd3f1c 261111-201156-5abf6c 261111-201159-483e21 261111-201202-8c50a4 261111-201206-73d52f 261111-201209-32089f 261111-201213-5cf20a 261111-201216-328810 (0 findings), 261111-201050-c7d247 (segment_test versions: 19 fixtures, 118/118 refusals; corpus replays), 261111-201230-8ef3e1 |
| A6 | 261111-223819-03f694 (reviewer's probe: ASan+UBSan rows and parity at Xapiand's pins, exit 0, 912 s); CI run 43153036741 at 2cc69d4: linux-asan-ubsan, linux-tsan and all platforms green (CI re-runs at 7a3a54b, docs only) |
| A7 | [artifact] `docs/gates/release-rows.md` |

## Result

`docs/gates/release-rows.md` (prospect): the pins, each row with its commit and jobs, the
oracle differences table. Harness `tools/release_rows/` (rows.sh, stem_parity.cc,
CMakeLists.txt); new test sections in `test/segment_test.cc` (versions per fixture) and
`test/aggregate_test.cc` (indexed one shard vs many); `tools/parity.cc` checks pruned = counted,
live and unique hits, and tallies by operator.

## Rulings I made

- The row-test step accepts both spellings of ctest's all-passed summary (with and without
  ", 0 tests failed"), anchored to the line and the eight tests.

- Rows run at the release's composition: prospect's library from this branch (its `include/`
  asserted equal to `850d4e8` in every job) with strata, compressors, hashes and threadpool at
  Xapiand master's pins through `FETCHCONTENT_SOURCE_DIR_*`; the harness builds from
  `tools/release_rows/CMakeLists.txt` because the root CMakeLists is outside the file set.
- A1's verify runs on the host at 1M documents in several segments, not the gate VM's 5M: the row
  is correctness across segments and merges, not scale (phase 3 kept the 5M row).
