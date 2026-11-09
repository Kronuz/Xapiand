# engine-search: journal

State: landed
Quest: .campaign/xapiand/quests/engine-search.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:cca0011bb on 2026-10-27 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/engine-search.md`: "`engine::Shard` reads: `stats` (uuid, revision, doc count" |
| A2 | `docs/status/engine-search.md`: "Migrated: `dump_documents` (both), `get_database_info`, `check`, `get_docid_term`" |
| A3 | `docs/status/engine-search.md`: "`harness/seam_check.sh` with `harness/seam_allowlist.txt` (per-file line ceiling, shrink-only, `--update`)" |
| A4 | `docs/status/engine-search.md`: "`qdiff.py` 2,007 answers and `qnf.py` 104 answers: `cmp` identical to the baseline." |
| A5 | `docs/status/engine-search.md`: "Revert checks, adapters (`.scratch/mutants4.py`): 12 of 12 caught by `test_engine_read`" |
| A6 | `docs/status/engine-search.md`: "`engine::search(std::span<Shard* const>, const SearchRequest&)` returns `task<SearchResult>`" |
| A7 | `docs/status/engine-search.md`: "Removed: `get_rset` (handler, both), `FilterPrefixesExpandDecider`, `get_edecider`, `Shard::db()`" |
| A8 | `docs/status/engine-search.md`: "Corpora, `cmp` identical to the baseline: `qdiff.py` 2,007, `qnf.py` 104, `qread.py` 911." |
| A9 | `docs/status/engine-search.md`: "`Shard::retry_read` (13 read ladders and `get_docid_term`, `get_view`, both metadata reads)" |
| A10 | `docs/status/engine-search.md`: "Spellings: `Xapian::rev/docid/termcount/doccount/valueno` are `engine::` in every MIT file" |
| A11 | `docs/status/engine-search.md`: "Mutants (`.scratch/x4c/mutants6.py`, each rebuilt and judged by the unit tests): 30 of 30 caught" |

## Result

Landed at Xapiand:cca0011bb on 2026-10-27. X4a: 16 of 16 unit tests; qdiff 2007, qnf 104, qread 911 answers identical. Reviews: `docs/reviews/engine-search-x4a-code.md`, `docs/reviews/engine-search-x4b-code.md` and `docs/reviews/engine-search-x4c-code.md` -- each APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase0.5/engine-search-c' (X4c).
- X4a: E2E 339 requests, 576 of 576 assertions; remote cluster range GREEN; seam 38 / 701.
- X4b: `test_engine_search` 3497 checks; 17 of 17 unit tests; qdiff 2007, qnf 104, qread 911, qsim 265 identical.
- X4b: E2E 339 requests, 576 of 576 assertions; cluster range 30 checks, 0 failures; seam 38 / 622.
- X4c: `test_engine_error` 243 checks; 18 of 18 unit tests; cluster range 30 checks, 0 failures.
- X4c: qdiff 2007, qnf 104, qread 911, qsim 265 identical; E2E 339 requests, 576 of 576 assertions.
- X4c review: malformed text query responses changed 10 of 242 from 500 to 400; C1, C2, N1, N5 addressed.
- Open: remote GPL edits remain with lead; TSan, Linux, FreeBSD, benchmarks and libFuzzer not run.
- Docs: `docs/design/xapiand-on-strata.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
