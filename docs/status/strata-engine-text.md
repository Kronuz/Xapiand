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

1. `Xapiand:f29dfe942` host-side `engine::parse_text`, Text node removed, errors mapped, changelog and
   mechanism note (`docs/mechanisms/text-queries.md`). Pin queryparser `09a4ea6`.
2. Test braces commit after it. Proven so far, ASan+UBSan build `.scratch/build-asan` (Apple clang):
   `xapiand_test_text` 7165 checks (706 A/B decisions as recorded; lowering equals the tree on ~5,500
   vector rows), `xapiand_test_engine_query` 22395 checks (live agreement with Xapian's parser),
   `seam_check.sh` green (7 files, 158 lines: unchanged; the text path never appeared in the allowlist).
3. Finding: `_near` over `(barack OR michelle) AND obama` (no BOOLEAN flag) is a QueryParserError: 400,
   asserted in `test_near_of_a_boolean_expression`. Stop words only: a group of only stop words keeps
   its words (as Xapian); only a text without words is empty.

## Resumed (after master gained S1)

4. Merged master into the branch (`CMakeLists.txt`: queryparser and prospect both linked). `seam_check.sh`
   green (7 files, 158 lines, unchanged). Proof, ASan+UBSan Apple clang, own build dir:
   - `xapiand_test_text` 7172 checks (706 A/B decisions unchanged), `xapiand_test_engine_query` 22395.
   - Docs E2E on the Xapian engine, ASan server: 576/576, 339 requests, uncovered 0.
   - Server probe: `_near` and `_phrase` over `(barack OR michelle) AND obama` answer 400; `_and`, plain
     `_near`, `-barack`, `michelle` answer 200 with the right hits. No AddressSanitizer report. UBSan
     reports only null-offset arithmetic in lz4.c and rapidjson stack.h (third party, not the text path).
   - Fuzz of the lowering: 60 s + 2 x 120 s (seeds 1, 2, 3), 1.7M texts, no violation.
   - Fuzz finding (oracle, not the lowering): Xapian builds some all-`*`/`?` wildcards (`*?`, `**`) as
     match-all and keeps others (`??`, `*??`); the oracle now reads all of them as match-all on both sides.
     Open for the strata engine: it must pick the same match-all patterns or not (lead).
   - Mutants of `src/engine/text.cc` (failing checks): window lost 318; operands swapped 2790; unsupported
     not a client error: abort (uncaught bad_alloc); single `?` flag lost 2 (was surviving; new
     `test_wildcard_pattern_flags`); unsafe format string: abort (format_error).
   - The changelog bullet already matches the probe (400).

## Next

Nothing on S4 but the lead's decisions: `cjk_words` segmenter (decision 2) and the all-wildcard patterns.

## Blocked on

Nothing.
