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
   - Fuzz finding, ruled by the lead: an all-`*`/`?` pattern is match-all iff it has a `*` and at most one `?`
     (`**`, `*?`, `?*`); `??`, `*??` filter by length. `leaf()` in `src/engine/text.cc` now lowers the
     match-all shapes to `Query::match_all()` on the host, so both engines get the same engine query (the
     strata engine has no wildcard lowering yet). A pattern behind a field prefix is never match-all. A lone
     `*` or `?` parses to no words. `test_all_wildcard_patterns`; its mutants (`<= 2`, `<= 0`, no `*`
     needed) each fail 2 checks. Fuzz after: 120 s seed 4, 835k texts, no violation.
   - Mutants of `src/engine/text.cc` (failing checks): window lost 318; operands swapped 2790; unsupported
     not a client error: abort (uncaught bad_alloc); single `?` flag lost 2 (was surviving; new
     `test_wildcard_pattern_flags`); unsafe format string: abort (format_error).
   - The changelog bullet already matches the probe (400).

## Security fix round (SR1, SR2; review `docs/reviews/strata-engine-text-security.md`)

`Xapiand:3612b7a57`. ASan+UBSan build `.scratch/s4-fix/build` (Apple clang, own deps), probes in `.scratch/s4-fix/`.

1. SR1: `to_xapian` and `Query::description` are iterative (one explicit-stack loop over combine, append and
   scale). Revert checks: the old `to_xapian` and the old `description` each overflow the stack in
   `test_depth_cap` (a tree at the cap, on a 40 KB thread stack).
2. The matcher recurses by depth and by width, so iterative conversion alone is not enough. Measured with the
   caps lifted, ASan server, macOS worker stack, `{"body": {"_or": TEXT}}` (the `_or` modifier turns the boolean
   syntax on; a plain `{"body": TEXT}` is a bag of words, and the reviewer's `a`, `b`, `c` are stop words):
   - nested depth (alternating AND/OR brackets): 500 survives, 550 dies. `oralt`: 500 survives, 600 dies.
   - width (one flat text of matching words, same word or distinct): 12,000 survives, 14,000 dies, a 60 KB body.
   - DSL `_and` nesting: 60 levels survive, 80 die; JSON to MsgPack conversion of a body: 300 levels survive,
     500 die.
   Caps (`src/engine/query.h`, applied in the `engine::Query` factories, so text lowering and the DSL share them):
   depth 100 (Elasticsearch default 30; 5x under the measured 500), clauses 4096 (Elasticsearch
   `max_clause_count` default; 2.9x under 12,000). Past either: `QueryDepthError` (a `QueryParserError`), 400.
   Elasticsearch's `indices.query.bool.max_nested_depth` is 20 in the 8.0 docs and 30 in 8.19.
3. SR2: `QueryDSL::process` counts frames (`MAX_NESTING` 90, about 45 levels of `_and`; ASan crash at 80 levels, so
   1.8x), the `q=` expression is measured on its RPN queue before the nested MsgPack is built
   (`expression_levels`; copying a deep one recursed and crashed an ASan server at 2,000 levels), and
   `json_load` refuses bodies nested past 200 levels (`MAX_JSON_DEPTH`, 1.5x under 300) before parsing.
   All answer 400 with the limit in the message.
4. Proof: `xapiand_test_text` 13196 checks (nesting at and past the cap; `test_json_depth_matches_the_document`:
   3,000 random documents against the parsed depth), `xapiand_test_engine_query` 22411, `fuzz_text` 90 s seed 7
   (522k texts, no violation), docs E2E 576/576 (339 requests, uncovered 0). `harness/query_depth_check.py`
   (22 checks over HTTP on an ASan server: text at the cap 200, past it 400, 4095 words 200, 4097 words 400,
   DSL 44 levels 200 and 45 levels 400, JSON 1,000 and 12,000 levels 400, node alive). The reviewer's probes
   rerun: `p1_nesting.py` 1,000 to 100,000 (72 requests, plain and wrapped in `_or`), `p2_httpq.py`,
   `p6_dsl_nest.py`: every answer 200 or 400, no crash. Worst shapes at the cap (every operator, NOT, +, -,
   phrases, boosts) answer 200 or 400, node alive.
   Mutants: `>` to `>=` in the cap check aborts `test_depth_cap` (uncaught `QueryDepthError` at exactly the cap);
   dropping the escape handling in `json_depth` fails the random-document check 3 times at least. Revert checks of
   the caps: the lifted-cap build dies at the depths above, the capped build answers 400.

Findings outside this branch (not fixed here):

1. Text-field wildcard expansion has no host cap (unchanged from before S4; `TextConfig` has no wildcard field).
   Reviewer's numbers: no crash or runaway memory up to a million-byte pattern, but no cap enforced (`scale_1.out`).
   Follow-up: decide a `wildcard_max_expansion` like `partial` (50) and `fuzzy` (error on exceed).
2. Hunt list: the unreproduced SEGV in the matcher (`Xapian::Vec<EstimateOp*>::clear()` from
   `AndContext::postlist`, once in ~26,000 requests of `p4_errors.py` seed 1, ~150 s). The depth and width caps
   make a stack overflow in the matcher unlikely to be its cause but do not rule out a use after free there;
   re-run `p4_errors.py` on this build to watch for it.
3. `url_parser`: a `q=` URL near 64 KB (66,008 characters) is cut inside a `%XX` escape and `urldecode`
   (`url_parser.cc:106`, called from `QueryParser::init`) reads one byte past the buffer: ASan heap-buffer-overflow.
   Found by the probes of this round; belongs to the url_parser library. Repro: `SEARCH /t/?q=` with a percent-encoded
   query of that length on an ASan server.
4. A document nested 150 levels (`{"a":{"a":...}}`) answers 500 (`InvalidArgumentError: Term too long (> 245)`), not 400.
   Pre-existing. MessagePack and YAML bodies were not probed for nesting; `json_load` guards JSON only.

## Next

Route the findings above (wildcard cap, url_parser, deep field path 500, MessagePack/YAML body depth). Open for the lead: the `cjk_words` segmenter (decision 2).

## Blocked on

Nothing.
