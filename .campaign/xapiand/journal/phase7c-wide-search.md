# phase7c-wide-search: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-wide-search.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:4f1cc9a94 on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-wide-search.md`: "[x] `--max-indexes-per-search` removed (code, test, harness, docs, changelog)" |
| A2 | `docs/status/phase7c-wide-search.md`: "[x] A search walks its endpoints in batches of whole indexes, at most K shards, merging top-k as each batch ends" |
| A3 | `docs/status/phase7c-wide-search.md`: "[x] Identity test `tests/test_wide_search.cc` (22,301 checks)" |
| A4 | `docs/status/phase7c-wide-search.md`: "[x] A document weighs as in a search of its own index alone" |
| A5 | `docs/status/phase7c-wide-search.md`: "[x] Hit ids are 64 bits; tests at the old 32-bit edge, one batch and two" |
| A6 | `docs/status/phase7c-wide-search.md`: "[x] Patterns: `tests/test_index_pattern.cc` (glob, expansion over a directory tree, the limit)" |
| A7 | `docs/status/phase7c-wide-search.md`: "[x] `harness/wide_search_check.py check`: two servers (all in one batch, K = 4) agree" |

## Result

Landed at Xapiand:4f1cc9a94 on 2026-11-03. Release ctest 37 of 37; E2E 386 requests, 656 of 656; wide-search harness PASS. Review: `docs/reviews/phase7c-wide-search-security.md` -- REJECT on SR1.

- Merge: Merge branch 'phase7c/wide-search' (searches in batches of K; patterns with a bounded walk; 64-bit hit ids; nesting refused).
- `Xapiand:9201227c8`, `b6b454d9a`, `6d62d96db`, `ade61e145`, `126878418`, `f9150fd24`, `2533b89c3` recorded.
- Linux ASan+UBSan ctest 36 of 36; E2E 656 of 656.
- Fuzz: 1,196,998 glob runs and 257,842 expansion runs; no finding.
- SR1 cost probe: 32 backtracking patterns on 99,999 dirs now return 400 in 0.47 to 0.78 s; one walk at a time means queued patterns wait.
- Linux ASan+UBSan, security re-check, and 10,000-index gate run remain open.
- Docs: `docs/mechanisms/wide-search.md`, `docs/dev/testing.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
