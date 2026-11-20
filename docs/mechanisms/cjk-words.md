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
space also goes where a CJK run meets another word (`abc中文` becomes `abc 中文`). Everything
else is copied unchanged.

Both sides call it, so they read the same words:

1. Indexing: `GenerateTerms::text(..., cjk_words)` runs the generator on the split text.
2. Queries: `parse_text` with `text::CJK_WORDS` (set from the field's `_cjk_words` in
   `query_dsl.cc`) parses the split text, so a word of a query is a term of the index and a
   phrase of CJK words is a phrase of terms.

Hangul is not divided: ICU has no Korean dictionary, so a Korean word is a run between spaces.
The property is stored with the schema (`_cjk_words`) and cannot change once set.

## Differences from ICU

The segmenter's own vectors replay ICU's segments on Chinese, Japanese and Korean corpora in
its ctest (`analysis:docs/cjk-segmenter.md`); the words here are those segments, minus the ones
that are spaces or punctuation. No difference is known. The Xapiand-side comparison against
ICU's iterator (the pre-landing job) is recorded here when it has run.
