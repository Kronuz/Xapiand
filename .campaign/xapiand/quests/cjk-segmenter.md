# cjk-segmenter: A CJK word segmenter in Kronuz/analysis

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cjk-segmenter`.

## Outcome

Kronuz/analysis splits Chinese, Japanese and Korean text into words the way ICU's word break iterator does (UAX #29 rules plus a dictionary break engine over ICU's cjdict for Han and kana; Korean by its UAX #29 rules), with no ICU at runtime, so `cjk-words` can bring `_cjk_words` back on an MIT dependency (D43).

## Acceptance criteria

- A1 The segmenter's API (header in Kronuz/analysis) segments UTF-8 text into word spans; its unit tests cover the UAX #29 rule cases and the dictionary path: the `build-test` check
- A2 Word boundaries equal ICU's word break iterator (ICU as an oracle in tools/ or test/, never linked by the library) on a Chinese, a Japanese and a Korean corpus of at least 100k characters each, every difference listed with its count: jobs
- A3 [artifact] The dictionary's provenance and licenses (Unicode-3.0 for ICU's data, cjdict's BSD and its libtabe and IPADIC notices) are kept in the repository and named in the README: `NOTICE` or `LICENSE` text quoted in the row
- A4 Decoding untrusted text never reads out of bounds or loops: a fuzz target under ASan and UBSan, 5 minutes at the head: a job
- A5 Size and speed: the dictionary's bytes in the built library and segmentation throughput (MB/s) next to ICU's on the A2 corpora, measured at a named commit: a job
- A6 [artifact] How it works: `docs/` in Kronuz/analysis (a mechanism note)

## Contracts

Provides `analysis:cjk-segmenter`, consumed by `cjk-words`.

## Decisions

- D43: our own segmenter first; ICU only as the oracle.
- Findings that led here: `cjk-words` journal (Xapiand:040a84c73), Blocked on.
- Corpora must be redistributable or generated; record their sources.

## May land early

The UAX #29 rule engine and its tests, before the dictionary path.
