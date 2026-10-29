> Status: CURRENT (2026-10-28) SF of strata's fields-and-keys design; nothing supersedes it.

# strata-fields (SF, phase 7a)

- Design: `strata/docs/design/fields-and-keys.md` revision 1 (section 7 SF, D6, D7, D8, D12, A-X6, C1 to C4, FK-R6); X6's ledger
  `docs/status/seam-fields.md` ("for SF"); prospect `phase8/fields` (`docs/mechanisms/fields.md`, `fielddict.hh`).
- Branch / worktree: `phase7a/strata-fields` at `~/code/Kronuz/Xapiand-wt/strata-fields`, from master `9e99575fa`; local only.
  prospect pin: prospect main `44415b6` (pins strata main `cef4e59`, F3); it was `phase8/fields` `1c62009` before.
- State: done; ready for the lead to merge (F3 repinned, C2 closed).
- Lead rulings (2026-10-28):
  1. F3 (delete `index_options::key_prefix`, the key field selects the key leaf by role) is a separate developer's step. This branch keeps the
     key path behind one seam (`src/engine/strata/keys.h`: `stored`, `host`, `key_prefix`, `lead`) that F3 deletes. C2's grep is empty after F3, not before.
  2. Key shape: the host's `_id` is one field declared with role `key`; the key bytes are the id's type character plus its encoded id
     (`K`+keyword, `N`+sortable number), no `Q`; a query on the `_id` field is a key leaf because of the field's role. One small host
     helper (`document_key`, `field_term`, `query_field`, `key_placement_hash` in `database/utils.{h,cc}`) carries key bytes through
     `replace_by_key`, `erase_by_key`, `find`, `WriteResult.term` and `unserialise_term_id`; no forwarding shims. The Xapian engine spells the
     key field's terms with a leading `Q` internally (`engine/xapian/legacy_terms.h`), so its index is byte-identical.
- Design amendment (for the lead to copy into the design): **A-SF1.** At the seam the id field is `engine::KEY_FIELD` ("_id"). A document names
  its key with `set_key(key)` (key = type character + encoded id); terms the host adds under `KEY_FIELD` are the key's, hold no postings, and their
  wdf counts in the document's length (D5's id-term length). `Query::term|wildcard(KEY_FIELD, key bytes)` is a key leaf / key scan. Placement of a
  multi-shard index still hashes `"Q" + key` (`key_placement_hash`) so a shard keeps its documents. D6's "without a type character" is not kept:
  `WriteResult.term` round-trips the type through `unserialise_term_id`, and two id types with equal bytes stay two keys.
- Decisions taken here (flag any the lead wants otherwise):
  1. `Query::field(name)` (a wildcard with an empty pattern) on strata takes every field of the shard whose name starts with `name` (the
     Xapian reading, `field: "*"` over an object reaches nested fields); the schema walk X6 suggested is not built. Any other node matches
     the field's name exactly. `Query::field(KEY_FIELD)` lists the keys.
  2. A field's declaration records the kind of the first document that names it (boolean docs, terms freqs, text positions); later documents
     reference the id, so a different kind never conflicts. Prospect reads postings by term flags, not by the declaration.
  3. A phrase whose terms are in two fields matches nothing (positions are per field, D9; the existing `test_fields` case expects the empty answer).
  4. The WAL line of a strata document is format 3 (fields, the key apart from the terms); formats 1 and 2 are refused (unreleased engine).
- Done criteria (the lead's task; ticked with evidence):
  - [x] SF1 fields declared through the field table, (field, term) postings: `Xapiand:` 46e3bb8df; `declared` in `test_engine_strata` (kinds, key field once, reopen, replay).
  - [x] SF2 `_id` routes through the key field; no host-built key term, no id term in `inverted` (`fill` writes the key apart; `test_fields` key cases; the key field's frequency counts in the length, M5).
  - [x] SF3 / C2 `interim_terms.h` and `keys.h` deleted; `git grep -nE 'interim|key_lead|engine_key|term_key_prefix|key_prefix|\\xff' -- src/engine` is empty (2026-10-28, after the F3 repin). `flat_term` stays only in logs (`generate_terms.cc` L_GREY).
  - [x] SF4 engine tests on both engines: ctest 28 of 28 (Release, no LTO); `test_engine_fields` 1258, `strata` 9796, `search_strata` 231324 (Xapian compare on), `callbacks` 946, `text` 13252.
  - [x] SF5 `strata_callbacks_check.py`: 208 searches, 0 different; `numeric_range_check.py` on strata: 797 ranges, 0 wrong (and on Xapian); `query_depth_check.py` on strata: 17 FAIL lines, 16 of them refusals the engine already had (`validate()` in `search.cc`: PHRASE/NEAR/ELITE_SET of text, unchanged by SF) and "node alive 404", which the script prints on the Xapian engine too.
  - [~] SF6 docs E2E (newman 6.2.3): Xapian engine 576 of 576 (339 requests, none uncovered); strata engine 507 of 576. The 69 failures are 55 term-list refusals (R-P2: "keeps no term lists"; INFO requests and one `_terms` aggregation) and 14 searches the engine refuses (PHRASE 4, NEAR 4, ELITE_SET 6); no wrong answer. Before the stem fix (below) it was 497, with 9 text searches answering 0 hits and one wrong count. Master's strata count was not run (no master build), so "no regression against master-on-strata" rests on those refusals being code SF does not touch.
  - [x] SF7 ASan+UBSan (`build-asan`, Apple clang, RelWithDebInfo -O1, own `_deps`, `NO_XAPIAN_COMPARE=1`, `ulimit -s 65532`): fields 1258, strata 9796, search_strata 153616, callbacks 946, document 645, query 22442, shard 54, read 657, replay 17, error 244, generate_terms 50, text 13252, wal_replay 5; 0 ASan reports; UBSan only RapidJSON `stack.h:117` (pinned, known). TSan (`build-tsan`, own deps): fields, strata, search_strata 153616, callbacks, shard, replay: 0 warnings.
  - [x] SF8 mutants, each alone, `.scratch/mutants.py` (copy in the main checkout's `.scratch/strata-fields/`): M1 a field resolved to the wrong id (`strata` 13 checks, `search_strata` aborts); M2 a key written as a term under every field (`search_strata` `test_fields`, 1 check); M3 a kind mapped to the wrong postings (`declared`, 1); M4 the key leaf looked up without the lead (`search_strata`, 5); M5 the key field's frequency left out of the length (2664); M6 `Query::field` exact only (1); M7 the stem field not split off a parsed term (`test_text`, 4). All killed.
  - [~] SF9 SR5: `check_fields(table_size, key_field)` runs on every segment's dictionary at shard open, after a reopen and in the engine check (`Core::verify_fields`, `shard.cc`). Not at merge: the merge reads its inputs through strata's `open_context`, which carries no table, so only a library change reaches it (the merged output holds only ids of verified inputs). No test: a segment with an id outside the table needs a resealed file; prospect's `fielddict_test` mutants T and U cover the check itself. Cost to measure under T6: the call scans the pool's keys of each segment at every open.
  - [~] SF10 CR2: no third copy of the stem marker was needed on the write side, but the query side needed one (`engine/text.cc` `split_field`, 'Z' + name), which the `analysis` change removes with the other two. CR4: `declared` fails on a wrong kind (M3). CR6: partial-path fields are fields of kind `terms` with an empty term (`declared`: `c.`); nothing to change.
  - [x] SF11 how-it-works note `docs/mechanisms/strata-fields.md`; `strata-engine.md`, `strata-callbacks.md` and `seam-fields.md` updated.
- Done: `Xapiand:` 46e3bb8df (the engine on fields and keys), 89e0b9f7b (stem field of a parsed text), 3bacc26f1 (host key helper test); the ledger and notes with the last commit.
- Doing: nothing.
- Next: measure `verify_fields` at open (T6). Master-on-strata E2E count if the lead wants the diff.
- Blocked on: nothing.
- Findings folded in: X6's for-SF list: CR2 (above), CR4, CR6; the `DocView::terms` item is moot (refused on strata); `field: "*"` over an object is the name-prefix reading of `Query::field` (decision 1), not a schema walk; `index_partial_paths` as above. SR5 as SF9.
- Last verified (2026-10-28, Apple clang 17, no LTO): see the criteria; ctest 28 of 28; E2E and harnesses on the final binary (`3bacc26f1` plus a comment edit).

## Log
- 2026-10-28: branch from master `9e99575fa`; prospect pinned to `phase8/fields`; lead rulings 1 and 2 recorded above.
- 2026-10-28: first build compiled after two include fixes; tests moved to named fields (the strata engine refuses an empty field name); Xapian-only tests keep bare names and set their keys with `set_key`.
- 2026-10-28: docs E2E found 9 language-analysed text searches at 0 hits: the parser's stems lead with 'Z' and the field name, which only the Xapian join understood; fixed in `engine/text.cc` (89e0b9f7b), killed by M7.
- 2026-10-28: the sanitizer builds shared the worktree with the mutant runs; their objects were rebuilt (touched sources) before the runs recorded above.
- 2026-10-28: repin to prospect `44415b67` (strata `cef4e59`, F3). `keys.h`, `lookup_bytes` and `options.key_prefix` removed; the index opens with `options.keyed = true`. Release no LTO: ctest 28 of 28; callbacks check 208 searches, 0 different; numeric_range_check 797 ranges, 0 wrong; docs E2E (newman 6.2.3) Xapian 576 of 576, strata 507 of 576 (the same 69: 55 term-list refusals, 14 refused PHRASE/NEAR/ELITE_SET). ASan+UBSan (Apple clang, -O1, own deps, NO_XAPIAN_COMPARE=1, ulimit -s 65532): ctest 28 of 28, 0 ASan reports.
- 2026-10-28: code review (docs/reviews/strata-fields-code.md) approves with conditions. CR1 fixed: `reopen_index` drops the index when `verify_fields` throws, so the shard raises until a reopen succeeds, as after a failed open. CR2 fixed (wording above). CR3 (T6 open cost on merged segments) and CR4 (no engine-level test of the remote-protocol term path) are follow-ups.
