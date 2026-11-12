# release-rows: journal

State: waiting on jobs 261111-201050-c7d247 261111-201053-f77c3a 261111-201056-33db85 261111-201059-749f46 261111-201102-4af80d 261111-201230-8ef3e1 261111-201233-507d9b and the 24 fuzz jobs below
Quest: .campaign/xapiand/quests/release-rows.md (the lead's; never edit it)

## Doing

Head prospect `2cc69d4` (pushed: CI `asan` runs on it). Build job 261111-191950-45f111 at
`fca9ef6` passed all 8 row tests but asserted: `rows.sh build` grepped for ctest's
"100% tests passed, 0 tests failed out of 8" and the host's ctest prints "100% tests passed out
of 8". `2cc69d4` matches either form, whole line, 8 tests (checked against that log and a failing
summary). Everything resubmitted at `2cc69d4`.

| Job | Step | For |
|---|---|---|
| 261111-201050-c7d247 | build: row tests at the release pins, binaries | A2 A3 A5 |
| 261111-201053-f77c3a | parity (after build) | A1 A2 |
| 261111-201056-33db85 | verify (after build) | A1 |
| 261111-201059-749f46 | stems (after build) | A4 |
| 261111-201102-4af80d | fuzz-build (ASan+UBSan) | A5 |
| 261111-201230-8ef3e1 | suite: prospect 850d4e8 at strata 5d742b2 (rerun of 90d3cd at the head) | A2 A5 |
| 261111-201233-507d9b | build-test at main's pins (campaign verify) | |
| 261111-201105-74a86d 261111-201108-63232e 261111-201111-fb8de3 261111-201114-9f665c 261111-201117-334338 261111-201120-b8edb5 | fuzz segment seeds 1-6 | A5 |
| 261111-201123-48fad1 261111-201126-e1d239 261111-201129-9bc82b 261111-201132-4a9573 261111-201135-50a022 261111-201138-698b31 | fuzz postings seeds 1-6 | A5 |
| 261111-201141-365038 261111-201144-b67b2c 261111-201147-634ad4 261111-201150-c36d62 261111-201153-bd3f1c 261111-201156-5abf6c | fuzz values seeds 1-6 | A5 |
| 261111-201159-483e21 261111-201202-8c50a4 261111-201206-73d52f 261111-201209-32089f 261111-201213-5cf20a 261111-201216-328810 | fuzz fielddict seeds 1-6 | A5 |

Earlier: 261111-190824-90d3cd (suite) passed at c984384; 261111-192016-2a010a (build-test) passed
at fca9ef6; 261111-190759-a5108f failed on the U64 collision fixed in fca9ef6.

## Next

1. Read the jobs above; fold failures (an oracle difference: fix if small and in the file set,
   else list it with the query and both answers).
2. Fuzz: submitted (table above); read each for findings and peak RSS.
3. If fuzz_postings OOMs at strata 5d742b2 (fuzz/MEMORY.md: fixed at strata 71750940/94480d52,
   past the release pin), one 10-min run at main's pins for the A/B; Blocked on: the release's
   strata pin.
4. `docs/gates/release-rows.md` with every row, its commit and jobs.

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

Nothing yet.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |
| A5 | <no evidence yet> |
| A6 | <no evidence yet> |
| A7 | <no evidence yet> |

## Result

Not yet.

## Rulings I made

- The row-test step accepts both spellings of ctest's all-passed summary (with and without
  ", 0 tests failed"), anchored to the line and the eight tests.

- Rows run at the release's composition: prospect's library from this branch (its `include/`
  asserted equal to `850d4e8` in every job) with strata, compressors, hashes and threadpool at
  Xapiand master's pins through `FETCHCONTENT_SOURCE_DIR_*`; the harness builds from
  `tools/release_rows/CMakeLists.txt` because the root CMakeLists is outside the file set.
- A1's verify runs on the host at 1M documents in several segments, not the gate VM's 5M: the row
  is correctness across segments and merges, not scale (phase 3 kept the 5M row).
