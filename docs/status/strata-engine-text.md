# strata-engine-text (phase 7a, S4: the text node through queryparser)

Branch `phase7a/strata-engine-text` from Xapiand master `7efc3753e` merged with `phase0.5/queryparser-ab`
(`c310e0568`). Local only, not pushed. Design: `docs/design/xapiand-on-strata.md` row S4 and D6 (strata);
`docs/design/queryparser.md` (branch `design/queryparser`) Q4.

## State

In progress. queryparser is the only text parser and runs on the host: `engine::parse_text`
(`src/engine/text.{h,cc}`) reads a field's query string into an `engine::Query`. The `Text` node is gone
from the seam (`Query::text`, `QueryKind::text`, the Xapian `QueryParser` path, `XAPIAND_QUERYPARSER` and the
A/B build). Mechanism note: `docs/mechanisms/text-queries.md`.

## Decisions taken here (flag any the lead wants otherwise)

1. Error kinds: syntax, `unsupported` (NEAR/PHRASE over phrases) and `too_long` all become
   `QueryParserError` (`errc::syntax`, a client error, 400). `unsupported` was `Xapian::UnimplementedError`,
   which the old translation classed as `errc::internal` (500): a client mistake answered as a server error.
   `out_of_memory` is `std::bad_alloc`. Settings the parser cannot read (an unknown stemmer language, a
   default operator it has no word for) are `InvalidTextConfigError` (`errc::invalid`).
2. `WORD_BREAKS` (`cjk_words`, ICU builds only) is not read: CJK text on such a field is split like any other
   text. Latin text is unaffected. Indexing with `cjk_words` still uses Xapian's term generator
   (`src/engine/xapian/text.cc`), so a CJK query on such a field no longer matches word-break terms. Open for the
   lead: an ICU break-iterator segmenter in `engine/text.cc` closes it; the docs suite has no `cjk_words` query.
3. queryparser pinned at `09a4ea6` (main: the merit decisions "2.1 versus 2.0" recorded).

## Done

(commits listed as they land)

## Next

1. Build and prove (below).

## Blocked on

Nothing.
