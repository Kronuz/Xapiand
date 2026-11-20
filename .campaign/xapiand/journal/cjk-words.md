# cjk-words: journal

State: blocked on lead: one CMakeLists.txt line outside my pin-only rule; code written, never built
Quest: .campaign/xapiand/quests/cjk-words.md (the lead's)

## Doing

Segmenter choice settled (analysis:cjk-segmenter, pin moved to analysis d72eb65). Written, not built:
`_cjk_words` property (schema.cc/.h, reserved/schema.h; exclusive with `_cjk_ngram`),
`engine::text::cjk_words` (text.cc) feeding both `GenerateTerms::text` (trailing `cjk_words`
param) and `parse_text` (`text::CJK_WORDS`, set in query_dsl.cc), tests/test_cjk_words.cc,
docs/mechanisms/cjk-words.md, text-type.md. The helper alone compiled and ran in scratch against
analysis's lib (zh/ja/ko/mixed samples give the expected splits).

## Next

1. Lead: add `analysis::cjk` to `target_link_libraries(XAPIAND_OBJ PUBLIC ...)` (CMakeLists.txt ~1242,
   beside `analysis::analysis`). Nothing links without it.
2. Then: build-test job, e2e docs example, ICU-oracle job (When done), fill the Differences section.

## Blocked on

The link line above: my rules allow only the analysis pin line in CMakeLists.txt.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | none yet (needs the link line, then build-test, e2e) |
| A2 | none yet (tests/test_cjk_words.cc, build-test) |
| A3 | docs/mechanisms/cjk-words.md |

## Rulings I made

- Both modes exclusive: setting `_cjk_words` and `_cjk_ngram` together is a ClientError.
- Query side splits the text before queryparser (no queryparser change; its pin is not mine).
- `GenerateTerms::text` takes `cjk_words` as a trailing default so existing callers stand.
