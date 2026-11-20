# cjk-words: _cjk_words back, on Phase 5's segmenter

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cjk-words`.

Whole feature (replan-2026-11-20, section 1 F9). The segmenter landed (`cjk-segmenter`, D43).

## Outcome

`_cjk_words` is a schema property again: Chinese, Japanese and Korean text is split into words for
indexing and queries by Kronuz/analysis's segmenter, beside `_cjk_ngram`, as Elasticsearch's ICU,
Kuromoji, Nori and SmartCN analyzers and Quickwit's `chinese_compatible` and `multilang` tokenizers
do, under the release's MIT license. Master refuses the property as an invalid field, so nobody's
searches change silently meanwhile. Repository: Kronuz/Xapiand.

## Acceptance criteria

Gate: the suite (`build-test`, `e2e`).

- A1 `_cjk_words` is accepted; indexing and queries split CJK text into words, and a search by word
  finds the document: tests in the `build-test` check and a docs example in the `e2e` check
- A2 The words Xapiand indexes equal the segmenter's own vectors on a Chinese, a Japanese and a
  Korean sample: tests in the `build-test` check
- A3 [artifact] `docs/mechanisms/cjk-words.md`

## Tasks

One agent, economical, medium (normal risk). Before starting, the lead narrows `src/**` and
`harness/**` to the schema and term-generation files it touches.

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `words` | the schema and term-generation files, `tests/test_cjk_words*`, `harness/**` example | implementation, economical, medium | The property, indexing and query splitting through the segmenter (A1, A2) |
| 2 | `docs` | `docs/mechanisms/cjk-words.md`, `docs/src/content/docs/**` | documentation, economical, medium | The note and the docs page (A3) |

## Contracts

Consumes Kronuz/analysis's segmenter (`analysis:cjk-segmenter`).

## Decisions

- D40: after the release; master's refusal is loud. It is ready now, and the lead may move it before
  the release when a slot is free (D39).
- The behavior-changes audit's #11: an agent removed it with Xapian's ICU, not the owner.
- The license is the release's audit re-run (`release` A6).

## When done

Normal risk: the lead reads the diff. Runs once, before landing: one light job comparing word
boundaries against ICU's word break iterator (the oracle D43 allows) on the three corpora, every
difference listed in `docs/mechanisms/cjk-words.md`.

## May land early

None.
