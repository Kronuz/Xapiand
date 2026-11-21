# `_cjk_words`

`_cjk_words: true` on a text field indexes and queries Chinese, Japanese and Korean text as
words. `_cjk_ngram` is the other mode (unigrams and bigrams); a field takes one of them, and
setting both is a client error.

## How it works

```
 text --> engine::text::cjk_words --> text with spaces between words --> term_generator / queryparser
```

`engine::text::cjk_words` (`src/engine/text.cc`) walks the text and sends each maximal run of
CJK word characters (`analysis::is_ngram_char`) to `analysis::cjk::segmenter`, which splits it
as ICU 78.3's word break iterator does. The words of the run are joined by single spaces, and a
space also goes where a CJK run meets another word (`abc中文` becomes `abc 中文`). A letter infix
(`&`, `'`, U+2019, U+00B7, U+2027 and the rest of `analysis::detail::infix_of`) beside a CJK
character gets a space on each side, because the term generator joins words across it: `乔治·华盛顿`
becomes `乔治 · 华盛顿`, while `AT&T` and `O'Neil` keep theirs. Everything else is copied unchanged.

Both sides call it, so they read the same words:

1. Indexing: `GenerateTerms::text(..., cjk_words)` runs the generator on the split text.
2. Queries: `parse_text` with `text::CJK_WORDS` (set from the field's `_cjk_words` in
   `query_dsl.cc`) parses the split text, so a word of a query is a term of the index and a
   phrase of CJK words is a phrase of terms.

Hangul is not divided: ICU has no Korean dictionary, so a Korean word is a run between spaces.
The property is stored with the schema (`_cjk_words`) and cannot change once set; switching an
existing field needs a new index.

A query is segmented on its own and a document in its context, so a word search is as good as the
dictionary: `京都` does not match `東京都`, which the segmenter reads as `東京 都` (Elasticsearch's
dictionary analyzers behave the same). The dictionary (2.09 MB, analysis's
`docs/cjk-segmenter.md`) is linked into every binary, whether or not a field sets `_cjk_words`.

## Differences from ICU

`tests/test_cjk_words_oracle.sh` runs ICU 78.3's word break iterator and the segmenter on the three
corpora of `analysis` (pinned commit) and lists every boundary or kind that differs. The words
Xapiand indexes are the segmenter's segments that are not spaces or punctuation, so equal
boundaries mean equal words.

| Corpus | Characters | ICU boundaries (words) | Ours | Differences |
|---|---|---|---|---|
| Chinese | 109,863 | 76,814 (54,208) | 76,814 | 0 |
| Japanese | 110,165 | 74,329 (65,531) | 74,329 | 0 |
| Korean | 110,015 | 60,909 (27,803) | 60,909 | 0 |

No difference was found. Korean agrees because both leave Hangul undivided.
