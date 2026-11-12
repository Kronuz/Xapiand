# release-rows: journal

State: waiting on jobs 261111-191950-45f111 261111-191957-551959 261111-192004-29c1c9 261111-192011-19892f 261111-192016-2a010a 261111-190824-90d3cd 261111-190918-66d76a
Quest: .campaign/xapiand/quests/release-rows.md (the lead's; never edit it)

## Doing

Head prospect `fca9ef6` (pushed: CI `asan` runs on it). `c984384` failed to build: xxhash's global
`U64` (lz4, through prospect's writer) collided with aggregate_test's slot enum; renamed to `S_*`.
The fuzz jobs submitted at `c984384` were cancelled so A5's fuzzing runs once, at the head the row
tests pass at.

| Job | Commit | Step | For |
|---|---|---|---|
| 261111-191950-45f111 | fca9ef6 | build: row tests at the release pins, binaries | A2 A3 A5 |
| 261111-191957-551959 | fca9ef6 | parity (after build) | A1 A2 |
| 261111-192004-29c1c9 | fca9ef6 | verify (after build) | A1 |
| 261111-192011-19892f | fca9ef6 | stems (after build) | A4 |
| 261111-192016-2a010a | fca9ef6 | build-test at main's pins (campaign verify) | |
| 261111-190824-90d3cd | c984384 | suite: prospect 850d4e8's whole suite at strata 5d742b2 (builds only the pin ref, so the result stands for any head) | A2 A5 |
| 261111-190918-66d76a | c984384 | fuzz-build (does it build at the release pins) | |
| 261111-190759-a5108f | c984384 | failed: the U64 collision above | |

## Next

1. Read the jobs above; fold failures (an oracle difference: fix if small and in the file set,
   else list it with the query and both answers).
2. When build passes and no code change is owed: `rows.sh fuzz-build` (--minutes 30, the five
   --ref pins as in build) and 24 x `rows.sh fuzz TARGET SEED` (segment, postings, values,
   fielddict; seeds 1-6; --minutes 15; --after fuzz-build; --for A5).
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

- Rows run at the release's composition: prospect's library from this branch (its `include/`
  asserted equal to `850d4e8` in every job) with strata, compressors, hashes and threadpool at
  Xapiand master's pins through `FETCHCONTENT_SOURCE_DIR_*`; the harness builds from
  `tools/release_rows/CMakeLists.txt` because the root CMakeLists is outside the file set.
- A1's verify runs on the host at 1M documents in several segments, not the gate VM's 5M: the row
  is correctness across segments and merges, not scale (phase 3 kept the 5M row).
