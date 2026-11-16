# cjk-words: _cjk_words back, on Phase 5's segmenter

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cjk-words`.

## Outcome

`_cjk_words` is a schema property again: Chinese, Japanese and Korean text is split into words for
indexing and queries, as Elasticsearch's ICU, Kuromoji, Nori and SmartCN analyzers and Quickwit's
`chinese_compatible` and `multilang` tokenizers do; `_cjk_ngram` stays beside it. The segmenter is
the ICU or UAX #29 analyzer Phase 5 builds in Kronuz/analysis (`phase5` A1), or one built there first
in its own quest, under the release's MIT license. It returns after the release (D40): master refuses
the property as an invalid field, so nobody's searches change silently meanwhile. Repository:
Kronuz/Xapiand.

## Acceptance criteria

- A1 `_cjk_words` is accepted again; with it, indexing and queries split CJK text into words: tests
  in the `build-test` check and a docs example in the `e2e` check
- A2 Word boundaries equal ICU's word break iterator, alpha.10's segmenter, on a Chinese, a Japanese
  and a Korean corpus, every difference listed: jobs
- A3 The segmenter and its dictionaries pass the license audit with no blocker for an MIT release
  (`license-closure`'s audit): a job
- A4 [artifact] How it works, with A2's comparison: `docs/mechanisms/cjk-words.md`

## Contracts

Consumes Kronuz/analysis's segmenter.

## Decisions

- D40: after the release; master's refusal is loud, so the release ships without it.
- The behavior-changes audit's #11: an agent removed it with Xapian's ICU, not the owner.
- ICU's data is large: A3 and the segmenter's size decide between ICU and a dictionary segmenter.

## May land early

None.
