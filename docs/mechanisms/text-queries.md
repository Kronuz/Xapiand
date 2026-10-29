# Text queries: one parser, on the host

> Status: CURRENT (2026-10-27) as built on `phase7a/strata-engine-text`; strata's S4 row of `docs/design/xapiand-on-strata.md`.

## What

The query string of a text field (`{"title": "barack AND obama"}`, `_near`, `_phrase`, wildcards, `term~`) is read by
`queryparser` (github.com/Kronuz/queryparser) into an `engine::Query`. `engine::parse_text`
(`src/engine/text.{h,cc}`) is the one entry; `QueryDSL::get_in_query` (`src/query_dsl.cc`) calls it with the
field's `engine::TextConfig` and gets a query tree of terms, wildcards, edit-distance terms and combinations.
The engine seam has no text node: the Xapian engine translates that tree with `to_xapian`, and the strata
engine will read the same tree.

## How

```
field text + TextConfig --> queryparser::parse --> tree (node ids, no recursion)
                                                    |
                  iterative lowering (explicit stack, bottom-up)
                                                    v
                                              engine::Query --> to_xapian | strata
```

`TextConfig` carries the field, the default operator, the `text::` flags, the stop-word and stemmer
languages, the stem strategy and the partial and fuzzy expansion limits. `parse_text` maps them onto
`queryparser::config` (stop words from `getStopWords`, one `snowball::stemmer` per language and thread) and
lowers the returned tree with an explicit stack: the call stack does not grow with the nesting of the text.
Nodes become `Query::term` (the field's name and the rest of the term, split where the text leads with the field), `wildcard`, `edit_distance`, `scale` and `combine`; a binary AND, OR, AND_NOT, XOR
or AND_MAYBE is built as a binary combine, the rest as a list with the window of NEAR and PHRASE.

Errors: a syntax error, an operator that cannot take its operand (NEAR and PHRASE take terms only) and an
over-long text raise `QueryParserError` (`src/exception_engine.h`): a `ClientError` (HTTP 400) that is also an
`engine::Error` of kind `syntax`. Settings the parser cannot read raise `InvalidTextConfigError` (kind
`invalid`). Memory exhaustion is `std::bad_alloc`. A text without words (empty, or only punctuation) lowers to a query that
matches nothing; a group of only stop words keeps its words, as Xapian's parser kept them.

## Why

Xapian's parser could not run on strata and put a Xapian dependency in every request. Parsing once on the host
gives both engines one reading, and `queryparser` builds its tree iteratively, so a deeply nested or
operator-heavy text cannot overflow the stack (the vendored parser corrupted the heap on
`{"t_some":{"_near":"(barack OR michelle) AND obama"}}`).

Where queryparser reads a text differently from Xapian 2.0, the decision is recorded per row in
`test/vectors/parse-ab.jsonl` in the queryparser repository (706 rows) with the reason; this repository
replays all of them through the lowering (`tests/test_text.cc`).

## Limits

1. `WORD_BREAKS` (`cjk_words`, ICU builds) is not read; indexing with it still uses Xapian's term generator.
2. A query is at most `MAX_QUERY_DEPTH` (100) levels of operators and `MAX_QUERY_CLAUSES` (4096) nodes
   (`src/engine/query.h`); the factories of `engine::Query` refuse a larger one with `QueryDepthError`, a
   client error (400). Xapian's matcher recurses over a tree by depth and by width, so the caps are
   what keeps a query from overflowing the matcher's stack; `to_xapian` and `Query::description` are
   iterative and do not depend on them. The matcher also recurses once per phrase or near group that an
   AND or FILTER joins, however flat the query is, so each node carries its `chain` (a phrase or near group
   is 1; AND and FILTER add their operands, every other operator takes the deepest) and `MAX_QUERY_CHAIN`
   (256) bounds it. The query DSL bounds its own recursion (`QueryDSL::MAX_NESTING`, 45 to 90 levels of
   nested clauses, and the `q=` expression before it is built). JSON, YAML and MessagePack bodies are
   refused past `MAX_JSON_DEPTH` (200) before they are copied: `json_load` scans the text, `yaml_load`
   counts libyaml's open collections, `msgpack_load` and the streaming unpacker use msgpack's depth limit.

## Proof

`tests/test_text.cc` (A/B decisions, lowering equals the tree on 5,500 vector rows, errors, nesting),
`tests/fuzz_text.cc` (random and grammar-mutated texts under random settings, each checked against
queryparser's description of its tree), `tests/test_engine_query.cc::test_text` (agreement with Xapian's
parser for the settings Xapiand uses), and the docs E2E suite. Ledger: `docs/status/strata-engine-text.md`.
