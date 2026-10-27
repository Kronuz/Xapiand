# seam-census (phase 0.5: census on the native document, amendment A-T5)

Branch `phase0.5/seam-census` from `phase0.5/seam-write` (Xapiand:879cd301a), with Xapiand master (Xapiand:829a67308) merged.
Local only, not pushed, no CI run.

## State

Done and proven locally; ready for review.

## Done

1. Merge (Xapiand:03330fe46). `shard.cc` conflicts resolved (census hook beside `xapian_document(doc)`). The census reads a
   document through `Document::Native::census_terms` and `census_values`, declared and implemented only under `ENGINE_CENSUS`.
   Default build: `nm` finds 0 census symbols, `strings` 0 `XAPIAND_ENGINE_CENSUS`, `size` segments equal to seam-write's binary
   (`__TEXT` 6627328, `__DATA` 2220032); the census build is 8651200 bytes against 8613120.
2. A-T5 (Xapiand:be13bc961). A numeric `_id`'s accuracy terms lead with `A` (`acc_term_prefix`, used at the three places that
   prepend the field prefix in `schema.cc`). Range queries over `_id` keep working.
3. Proof.
   1. Docs E2E 576 of 576 on Release and on ASan+UBSan (newman 6.2.3; UBSan reports only the known rapidjson pointer-overflow
      in `stack.h`).
   2. Census run over the suite: 2625 documents, 0 with more than one `Q` term (before: 2 with 7); 473 named-key and 2152
      keyless documents hold exactly one.
   3. ctest 19 of 19 under ASan+UBSan. Release: 18 of 19; `test_offload_followups` aborts at start with `condition_variable wait
      failed: Invalid argument`. Its code (reactor, node, drain) is untouched by this branch (`git diff master -- src` outside
      `src/engine`, `src/database`, `src/server` is empty); it passes under ASan. Not run against master.
   4. `harness/seam_check.sh` exit 0, 7 files, 158 lines (allowlist unchanged).
   5. SEARCH dump (bank, 1000 accounts) and `:info` hashes identical to seam-write's binary (`fc56819c...`, `8440196...`); a
      numeric-`_id` bank with 5 range queries over `_id` hashes identically on both binaries (`cfae6c3b...`); the terms differ (A-T5)
      and the answers do not.
   6. Tests: `test_engine_census` 72 checks (new document-shape test), `test_generate_terms` 10 (new `test_id_accuracy_prefix`),
      `harness/id_terms_check.py` (20 range queries; with `--census` the T5 count).
   7. Mutants: positioned-term test (`positions == 99`) fails 1 census check; A-T5 reverted (`acc_term_prefix` returns the field
      prefix) fails 3 unit checks and the harness (87 documents with more than one `Q` term); query side only reverted fails
      the harness (range queries over `_id` return nothing).
   8. Bulk smoke (20000-document RESTORE, bank, one node, 9 interleaved rounds, screen only): 4472 and 4488 docs/s for
      seam-write's binary, 4344 and 4491 for this branch's Release build; the first run had one 3880 outlier. The bank has no
      numeric ids, so A-T5 is not on its path.

## Next

1. Review (code reviewer): the `Document::Native` additions and the three `acc_term_prefix` sites.
2. Existing indexes with a numeric `_id` need reindexing for range queries over `_id` (changelog entry added).
3. The strata engine adds `census_terms` and `census_values` to its document, and the same `A` prefix.

## Blocked on

Nothing.
