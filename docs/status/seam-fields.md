> Status: CURRENT (2026-10-28) X6 of strata's fields-and-keys design; nothing supersedes it.

# seam-fields (X6, phase 7a)

- Design: `strata/docs/design/fields-and-keys.md` revision 1 (section 7 X6, D2, D9, D12, D14; conditions C3, C4), review
  `strata/docs/reviews/fields-and-keys-design.md` (approve with conditions, re-check closed FK-R1 to FK-R6).
- Branch / worktree: `phase7a/seam-fields` at `~/code/Kronuz/Xapiand-wt/seam-fields`, from Xapiand master `5d7f3263d`;
  local only, not pushed, no CI run. Baseline (master) build for the comparisons: `Xapiand-wt/seam-fields-base`.
- State: code review done (approve with conditions, `docs/reviews/seam-fields-code.md`); CR1 and CR3 fixed, CR2/CR4/CR5/CR6 recorded for SF; merged with S2; merged to master (local, not pushed).
- Done criteria (from the lead's task; evidence in Last verified):
  - [x] X1 `engine::Document` and the query nodes carry a field; the Xapian engine joins (field, term) into today's term.
  - [x] X2 Every site of D12's counts migrated; `git grep 'prefixed('` is empty (the document key and logs use `flat_term`).
  - [x] X3 SEARCH dumps, `:info` hashes and the shard term dump equal master's on the Xapian engine.
  - [x] X4 Docs E2E 576 of 576.
  - [x] X5 The engine census unchanged (one explained counter, below).
  - [x] X6 `harness/seam_check.sh` green (exit 0, files=7 lines=158, equal to master).
  - [x] X7 The engine tests on both engines (ctest 25 of 25).
  - [x] X8 ASan+UBSan on the engine tests.
  - [x] X9 A mutant that maps a field to the wrong prefix fails a test (ten mutants, all killed).
  - [x] X10 C4: the interim mapping is `src/engine/strata/interim_terms.h`, the only place the strata engine joins a field and a
        term. C3: the adapter grep (`key_lead|engine_key|term_key_prefix|key_prefix` in `src/engine/strata`) is 9 lines on
        master and 9 here.
  - [x] X11 `docs/mechanisms/seam-fields.md`.
- Done:
  1. Xapiand:a919deb6f the seam speaks (field, term): `Document::field`/`FieldRef`/`FieldKind`, `Query::term|wildcard|
     edit_distance|field`, `TextConfig::field`; Xapian join (`convert.cc`, `query.cc`, `text.cc`, the `V` version term as
     field `V`); strata interim join; callers in `schema.cc`, `generate_terms.cc`, `query_dsl.cc`, `search_views.cc`;
     `tests/test_engine_fields.cc`; the mechanism note.
  2. Xapiand:65a3df348 `harness/fields_identity.py`.
- Doing: nothing.
- Next: code review; then the S2 to S4 branches rebase onto this (their `add_term`, `Query::term`, `term_key_prefix` sites are
  SF's); SF resolves fields to ids and deletes `interim_terms.h`.
- Blocked on: nothing.
- Findings folded in: CR1 (merge with S2: `search.cc` joins `interim::flat_term(field, str)`, match-all is an empty field and term;
  strata has no edit distance or wildcard, so no prefix length to adjust; `test_fields` in `test_engine_search_strata`), CR3 (debug
  assert in `Document::at`, `test_foreign_ref_asserts`). CR2, CR4, CR5, CR6 recorded below.
- For SF (from the code review `docs/reviews/seam-fields-code.md`; none needs another review):
  1. CR2: the stem field is found by a leading `Z` in two copies (`xapian/text.cc`, `generate_terms.cc`); the `analysis`
     change that reports the stem flag removes both. Do not add a third copy.
  2. CR4: `Field::kind` and its widening have no consumer on either engine; Schema's choices (`schema.cc`: positions mean
     `text`, `bool_term` means `boolean`, else `terms`; accuracy and geo terms default to `terms`) are unverified against D12.
     SF adds the consumer and a test that fails on a wrong kind.
  3. CR5: `set_key(key)` keeps the host building the key term (open item 2): a design amendment for SF's shard key path.
  4. CR6: `index_partial_paths` (`schema.cc`) declares the whole partial-path term as a field name with an empty term; on
     strata after SF that is one field per path, so check their types then.
- Open for the reviewer and SF (decisions I did not take):
  1. `DocView::terms(prefix, sink)` (aggregations `metrics.cc`) and `term_details` still hand out whole Xapian terms.
  2. Shard key operations take the opaque key string (`flat_term`), not (field, key); `Document::set_key(key)` is unchanged and
     has no `key` field kind. D12's `set_key(std::string key)` with the Xapian engine adding the `Q` term needs the id field's
     type character, which a key alone does not carry.
  3. `Query::field(name)` keeps today's prefix reading on Xapian (`field: "*"` over an object reaches nested fields; the null
     and `*` sites in `query_dsl.cc` pass a prefix with no type character). D2 says names match exactly, so SF builds that case
     from the schema.
  4. Text stems: the generator runs with no prefix and a leading `Z` marks a stem (words are lowercase). Exact, tested against
     the generator with the prefix for every strategy; the clean fix is the additive `Kronuz/analysis` change D12 names.
  5. The 245-byte term limit is checked on the joined length in `Schema::index_term`.
  6. The Xapian engine cannot tell ("ab", "c") from ("a", "bc"); pinned by `test_marker_and_joined_ambiguity`.
- Last verified (2026-10-28, Apple clang 17, Release without LTO, master `5d7f3263d` as baseline in `Xapiand-wt/seam-fields-base`):
  1. ctest 25 of 25 (new `test_engine_fields`: 1237 checks; `test_engine_strata` runs both engines; `test_wal_replay` both).
  2. `harness/fields_identity.py` base vs branch: search (80 queries on bank and cities, 1000 documents each, totals checked
     non-trivial), `:info` of 2000 documents, and the shard term dump (191352 lines): all three sha256 equal
     (`16e9938a...`, `9346383f...`, `bd7da062...`). `harness/terms_diff.py` (130 analysis configs, stems and stop words in 23
     languages): equal sha256 `0707691f...`.
  3. Docs E2E (newman 6.2.3): 576 of 576 on both; `e2e_diff.py` no status differences; `bodydiff.py` 2 divergent bodies, the
     volatile auto-id and `/:metrics` (the documented expected pair).
  4. Census (`ENGINE_CENSUS` builds, `e2e_capture.sh` + `census_report.py`): per-request tables equal. Differences: counts of
     background and call counters (`docview.data` 1047 base, 1043 base again, 1051 branch; `shard.commit`, `find_key`) are
     run-to-run noise of the same size on master against itself; `query.edit_distance.prefix_length` (1 request) is gone
     because the prefix length now counts from the term, so the fuzzy query's 0 is not counted (it was the field prefix's
     length before).
  5. ASan+UBSan (Apple clang, `-O1`, own `_deps`): `engine_fields` 1237, `document` 645, `query` 22395, `strata` 9559,
     `search` 4423, `shard` 54, `read` 657, `error` 244, `replay` 17, `generate_terms` 50, `wal_replay` pass on both
     engines; 0 ASan reports; the only UBSan lines are lz4's known zero-offset ones.
  6. Mutants (`.scratch/mutants.sh`, copy in the main checkout's `.scratch/seam-fields/`), each built and run against
     `engine_fields`, `engine_document`, `engine_query`: Xapian join drops the field (killed: uncaught Xapian error on the
     empty term, `engine_document` 2 checks); drops the type character (same); wrong prefix `x` + name (608 of 1237 and 597 of
     645 checks fail); query join drops the field (13 fail); query join wrong prefix (16 and 1496 fail); edit distance forgets
     the field's length (3 fail); the stem field is the word field (128 fail); stems keep their `Z` (128 fail);
     `Document::field` ignores the name (uncaught Xapian error); strata join reversed (uncaught `map::at`). All killed.
  7. No decoder or parser added or changed (the log format is untouched), so no new fuzz target; `fuzz_strata_document` does not
     use the changed calls.
  8. Not run: CI (nothing pushed); TSan (no new shared state: a `Document` is built by one thread); the cluster harnesses.

- Round after the review (2026-10-28, Apple clang, own build dirs and `_deps`, removed after):
  1. Merged master (S2 and the copyright commit) into the branch; S2's strata search tests moved to the (field, term) API.
  2. ASan+UBSan, RelWithDebInfo: `engine_fields` 1237, `document`, `query`, `search`, `strata`, `shard`, `read`, `error`, `replay`,
     `generate_terms` pass; `engine_search_strata` 153600 checks pass with `NO_XAPIAN_COMPARE=1` and `ulimit -s 65532` (S2's
     recipe: Xapian's own matcher use-after-frees under ASan; the model's recursion over the 20000-link chains needs the stack
     under Apple clang). 0 ASan reports; UBSan only rapidjson's zero-offset lines in `wal_replay` and `sort`. Debug ASan:
     `engine_fields` 1242 (the assert test is live only there), `document`, `query`, `strata`, `search_strata` pass;
     `engine_search` aborts inside Xapian's Debug matcher (S2 records the same family).
  3. CR1 revert (master's `search.cc`): `test_fields` fails 7 checks; mutant (match-all ignores the field): 2 fail.
     CR3 revert (assert removed): `test_foreign_ref_asserts` fails 4 checks.
  4. Docs E2E on the Xapian engine (Release, newman 6.2.3): 576 of 576, 339 requests, e2e_assert GREEN.
  Not run: `harness/seam_check.sh`, `fields_identity.py`, census (unchanged by this round except `search.cc`, a strata-only file).

## Log
- 2026-10-28: branch created from master `5d7f3263d`; seam, Xapian and strata natives, callers and tests edited;
  Xapiand:a919deb6f. First build: three compile errors (the `V` version term, two `string_view` arguments), fixed.
- 2026-10-28: identity, E2E, census, ASan and mutants run (above); Xapiand:65a3df348 the harness; build directories removed.
