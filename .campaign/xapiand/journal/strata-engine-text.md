# strata-engine-text: journal

State: landed
Quest: .campaign/xapiand/quests/strata-engine-text.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:7c5dca498 on 2026-10-28 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/strata-engine-text.md`: "`Xapiand:f29dfe942` host-side `engine::parse_text`, Text node removed, errors mapped" |
| A2 | open: CJK (`cjk_words`) query compatibility left open in `docs/status/strata-engine-text.md` |
| A3 | `docs/status/strata-engine-text.md`: "Fuzz of the lowering: 60 s + 2 x 120 s (seeds 1, 2, 3), 1.7M texts, no violation." |

## Result

Landed at Xapiand:7c5dca498 on 2026-10-28. Text query cap: depth 100 and clauses 4096; query-chain cap 256; body depth 200. Review: `docs/reviews/strata-engine-text-security.md` named.

- Merge: Merge branch 'phase7a/strata-engine-text' (S4: text queries through queryparser).
- Xapiand:f29dfe942 host-side parser, removed Text node, mapped errors, changelog, and mechanism note; queryparser pinned at `09a4ea6`.
- Xapiand:3612b7a57 iterative conversion/description and query, DSL, JSON depth caps.
- Xapiand:c1fca3f6c SR1b; Xapiand:a38904396 SR3; Xapiand:5fd538f82 field-path and `_fuzzy` fixes.
- Xapiand:0e01adabc converter termination fix, fuzz watchdog, and replay.
- `_fuzzy`: 12,000 words 3.02 s to 0.04 s; 14,000 4.23 s to 0.05 s; 100,000 timed out to 0.34 s (400).
- Final tests include `xapiand_test_text` 13243 and `xapiand_test_engine_query` 22442; docs E2E 576/576; Open: wildcard expansion cap, url_parser over-read, and queryparser fix; YAML alias amplification is closed by refusal.
- Docs: `docs/mechanisms/text-queries.md` named.
- Left open: A2.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
