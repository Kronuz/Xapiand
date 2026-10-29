> Status: CURRENT (2026-10-28) SF of strata's fields-and-keys design; nothing supersedes it.

# strata-fields (SF, phase 7a)

- Design: `strata/docs/design/fields-and-keys.md` revision 1 (section 7 SF, D6, D7, D8, D12, A-X6, C1 to C4, FK-R6); X6's ledger
  `docs/status/seam-fields.md` ("for SF"); prospect `phase8/fields` (`docs/mechanisms/fields.md`, `fielddict.hh`).
- Branch / worktree: `phase7a/strata-fields` at `~/code/Kronuz/Xapiand-wt/strata-fields`, from master `9e99575fa`; local only.
  prospect pin: `phase8/fields` head `1c62009` (prospect main had not landed F2 when this started); it pins strata `32b6800`.
- State: implementing (key path interim until F3, below).
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
  - [ ] SF1 the strata engine declares its fields through the field table and writes (field, term) postings.
  - [ ] SF2 `_id` routes through the key field; the host-built key term and the duplicate id term in `inverted` are gone.
  - [ ] SF3 `interim_terms.h` deleted; `git grep` for the interim join and the 0xff lead: `interim_terms.h` done; `keys.h` waits for F3.
  - [ ] SF4 engine tests on both engines.
  - [ ] SF5 `harness/strata_callbacks_check.py`, `numeric_range_check.py`, `query_depth_check.py` on strata.
  - [ ] SF6 docs E2E on the strata engine (count; zero regressions against Xapian).
  - [ ] SF7 ASan+UBSan and TSan in their own dirs with their own deps.
  - [ ] SF8 a mutant per claim (a field resolved to the wrong id; a key written as a term).
  - [ ] SF9 SR5: `check_fields(table_size, key_field)` wired at segment open, merge and `check_segment`.
  - [ ] SF10 CR2 (one stem detection), CR4 (a test that fails on a wrong `FieldKind`), CR6 (partial-path fields).
  - [ ] SF11 how-it-works note.
- Done: nothing committed yet.
- Doing: code written for the seam, the Xapian engine, the host and the strata engine; first build and tests.
- Next: tests on both engines, harnesses, E2E, sanitizers, mutants, note.
- Blocked on: F3 for deleting `keys.h` and the strata `key_prefix` option (lead has assigned it).
- Findings folded in: none yet.
- Last verified: nothing built yet.

## Log
- 2026-10-28: branch from master `9e99575fa`; prospect pinned to `phase8/fields`; lead rulings 1 and 2 recorded above.
