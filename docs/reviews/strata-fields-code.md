# Code review: phase7a/strata-fields (SF)

Branch `phase7a/strata-fields` (Xapiand:46e3bb8df, 89e0b9f7b, 3bacc26f1, 3bc851b17, 66c45f53c) on master 9e99575fa.
Reviewer build: Release, no LTO, Apple clang 17, one build (`build-sfrev`, deleted afterwards). Sanitizers not rebuilt (see Not run).

## Verdict: APPROVE WITH CONDITIONS

No blocker. Key identity holds for every id type and first byte on both engines, placement is byte-identical to master's hash, the Xapian engine adds `Q` exactly once, and the stem fix is correct and tested. One small error-path defect (CR1) and a ledger accounting gap (CR2) go to the fix round.

## Re-verified

1. ctest 28 of 28 (Release).
2. Docs E2E with newman 6 from `~/.scratch/newman6`: Xapian engine 576/576, strata engine 507/576 (69 failing), matching the ledger. Logs: `.scratch/cr_e2e_{strata,xapian}.log`.
3. Server-level A/B of ids on both engines, outputs identical: text-typed `_id` (exact, word, wildcard, `*`), positive `_id` (explicit, auto-assigned, 0, 2^64-1, delete, `*`). Scripts `.scratch/cr_idtext.sh`, `.scratch/cr_idnum.sh`, outputs `.scratch/cr_idnum_{xapian,strata}.out`.

## Probes (all in `.scratch/`)

- `cr_keys_probe.cc` (21108 checks, 0 failures). Links like `test_engine_fields`. Two parts:
  - Identity: for every first byte 0..255 times types K N U S T B (id of one byte and of byte+"x"), sortable numbers 0, 1, -1, 1e9, 2^32, -0.5, an id with embedded NUL: `document_key == type+id`; `key_placement_hash(key) == fnv1ah64::hash(flat_term(id,"Q",type))` (master's formula, so placement is unchanged); `key_of_term("Q"+key) == key`; `field_term("Q",t,id)` is (`KEY_FIELD`, key); `query_field("Q") == KEY_FIELD`.
  - Round trip on both engines: 256 first-byte keys, 4 numeric keys, a NUL key, the empty id; `replace_by_key`, `find_key`, `Query::term(KEY_FIELD,key)` returns exactly 1, `Query::field(KEY_FIELD)` lists all, `N`/`K` wildcards split correctly, same after commit + reopen, `erase_by_key` then `find_key == 0` and zero listed.
- `cr_open_probe.cc`: open time of a committed strata shard, 20k / 200k / 800k docs: 0.75 / 1.11 / 4.33 ms.

## Focus answers

1. Key identity: agrees (probe above; code at `src/database/utils.cc:71-113`, callers `handler.cc`, `schema.cc`, `shard.cc`). Placement still hashes `"Q"+key` at the five sites that used `fnv1ah64::hash(term)` on master (`handler.cc` 1557, 1657, 1996; `schema.cc` 2793, 2860), all moved to `key_placement_hash`. No remaining raw `fnv1ah64::hash` of an id term outside it. The numeric-id tests moved from `QN` to `N` consistently (`handler.cc:421,428,2133`, `schema.cc:2987`, `shard.cc:1546`).
2. Xapian `Q`: `legacy_terms.h` is the only place that adds it (`join_into`, `key_term`); every Xapian entry point (`add`, `replace`, `replace_by_key`, `erase_by_key`, `find_key`, census, query leaf, `XapianDocument::join`/`set_key`) goes through it once. Nothing strips it except `key_of_term` at the two remote-protocol receive sites, which is the right place. `edit_distance`'s prefix length uses `term_prefix(field).size()` so a key-field edit-distance counts `Q` as one char as before (covered by `test_key_field`). No double prefix found; the Xapian round trip in the probe passes.
3. `verify_fields`: see CR1.
4. Stem fix: correct. `split_field` tries the word field first, then `"Z"+field`, only when the strategy marks stems with `Z` (`some` or `all_z`) and a stem language is set. `test_stems_are_terms_of_the_stem_field` plus mutant M7 cover it, and the E2E went from 497 to 507 on it. Residual ambiguity: a word field named exactly `Z` makes a stem term `ZZ..` split as word field `Z` (prefix check order). Host field names are generated prefixes, so this is theoretical (CR4, note).

## Findings

### CR1 (non-blocking, fix round): a failed `verify_fields` in `reopen_index` leaves a usable corrupt index
`src/engine/strata/shard.cc:206-216`. `reopen_index` resets and emplaces the index, then calls `verify_fields()`, then sets `fenced = false`. If verification throws, the index stays emplaced and `fenced` keeps its previous value. `unusable()` is `fenced || !index`, so when the core was not fenced (the `cancel_transaction` path at line 855, or a core whose earlier reopen failed at open) the next call sees a usable core over a segment that failed the check. The comment at line 855 says it must "raise on every call until a reopen succeeds". The first-open path (`acquire`, line 357) is fine because the core is dropped. Fix: `index.reset()` (or set `fenced = true`) on a verify failure. Reachability is low (segments are immutable, so the first open already verified them); I did not build a probe because producing a segment that passes open and fails the field check needs a resealed file, which is also why the developer has no test for the check. Evidence is the code read, not a failing run.

### CR2 (non-blocking, ledger): the "69 = 55 INFO + 14 refusals" split is approximate
I reproduced 507/576. Besides the INFO requests and PHRASE/NEAR/ELITE_SET, the Terms Aggregation "Structuring / Field" request (`_terms` on `personality`) fails with 400 "The strata engine keeps no term lists" (probe `.scratch/cr_terms.sh`). Same R-P2 cause, same by-design refusal, but it is not an INFO request. Correct the ledger wording; no code change. "No regression against master-on-strata" is still unproven (no master build), as the ledger says.

### CR3 (non-blocking, T6): verify cost
Open measured 0.75 to 4.3 ms up to 800k docs in single-field unmerged segments, so the directory pass is cheap there. The pooled-keys scan only runs for segments that have a pool (merged segments); that shape was not measured. Measure on a merged segment under the many-indexes workload as T6 already plans.

### CR4 (note): no test for the remote-protocol term path
`key_of_term` is unit-tested, but `msg_deletedocumentterm` / `msg_replacedocumentterm` (`remote_protocol_views.cc`, not read per rules) and the `Xapian::Document` overload of `replace_document_term` (`database/shard.cc:1397`) are not exercised on either engine. On the strata engine a remote document would carry a literal `Q...` term via `from_xapian`; the engine's stated plan replaces the remote protocol, so this is only a note.

### Design note for the architect (A-SF1)
The host adds the key to the document twice, `set_key(key)` and a `KEY_FIELD` term with the same bytes; strata keeps only the first and counts the second as wdf for length. Any extra `KEY_FIELD` term with different bytes (as `test_key_field` builds) is silently without postings on strata but a real term on Xapian. Fine today because the host only adds the key itself; worth one sentence in the amendment.

## Criteria check

- SF1 to SF5, SF7, SF8, SF11: evidence matches (ctest and E2E re-run; mutant list is plausible and M7 tied to the stem test; I did not rerun mutants).
- SF6: the shortfall is the refusals plus CR2.
- SF9: error mapping is right (`corrupt()` throws `corrupt_segment`, mapped to `errc::corrupt` by `rethrow_translated`; `check()` returns it as a failure string); lifetime of `Pinned::fields_` is safe because the pin holds the core mutex (`pin.h`, `shard.cc:1148`). CR1 is the gap.
- Test strength: `test_host_keys` is close to a restatement of the helpers, but the round-trip probe above and the engine tests cover the real behavior; the helper test is not what protects placement.

## Not run

TSan and ASan/UBSan (relied on the developer's logs; no memory-safety suspicion arose in the changed code, which is mostly string plumbing), mutants M1 to M7, merge-time field behavior, a multi-shard server placement A/B against a master build (no master build; placement equality shown by hash equality instead), other platforms (CI).
