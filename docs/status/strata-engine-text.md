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
4. (Closed in the second round below: the 150-level field path is a 400, MessagePack and YAML bodies are depth-limited.)

## Security fix round 2 (SR1b, SR3, field path, `_fuzzy` cost; re-check 1 in `docs/reviews/strata-engine-text-security.md`)

`Xapiand:c1fca3f6c` (SR1b), `Xapiand:a38904396` (SR3), `Xapiand:5fd538f82` (field path, `_fuzzy`). ASan+UBSan build
`.scratch/s4-sr1b/build-asan` and a release build `build-rel` (own deps, deleted at the end); probes and scripts in
`.scratch/s4-sr1b/` (`bisect.py`, `bisect2.py`, `atcap.py`, `sr3.py`, `fz.py`, `allprobes.sh`).

1. SR1b, measured by bisection over HTTP on the ASan server (`-O1`, worker-thread stack, caps lifted by shape):
   the matcher recurses once per phrase or near group that an AND or FILTER joins. Crash at 601, 600 survive:
   AND and FILTER of `_phrase` (2, 3 or 5 words) or `_near`; AND of mixed phrases and near groups; AND of
   `(AND_NOT phrase x)`; AND of one-phrase ORs; AND of wildcard and phrase pairs (600). Chains add through an
   OR: AND(OR(AND of 299 phrases), AND of 299) survives, 300 each dies (598 and 600 joined). They do not add
   across siblings: AND_NOT and XOR of two ANDs of 682, an OR of 13 ANDs of 100, AND_MAYBE(AND 600, AND 600)
   survive, and AND_MAYBE with 601 on one side dies. Survive at the clause cap (4,095 nodes): AND of terms,
   wildcards, fuzzy leaves, and of 1,365 OR, AND or AND_NOT groups of terms or of 585 OR-of-two-phrases groups;
   OR, XOR, AND_MAYBE, AND_NOT of 1,365 phrases or near groups; scaled phrases in an AND (1,023); text `_max`,
   `_synonym`, `_elite_set` of phrases nest one level per phrase, so the depth cap (100) bounds them.
   Limit: `QueryNode::chain` (a phrase or near group is 1; AND and FILTER add, every other operator takes the
   deepest operand), refused past `MAX_QUERY_CHAIN` = 256 in `set_extent`, a 400. Margin under ASan 2.3x (601 to
   256); the shapes at the limit (nested through OR, AND_NOT, AND_MAYBE, FILTER, 20-word phrases, near groups of 5)
   answer 200 on the ASan and the release server (`atcap.py`). Release sanity: the pre-S4 release binary
   (`.scratch/build`, no caps) survives 2,942 five-word phrases and dies at 2,943 (nested through an OR: 1,470 each
   side survive, 1,471 die), so the release margin is 11x. The release build of this branch answers the harness
   and the at-limit shapes with 200 and 400, node alive.
2. SR3: MessagePack bodies unpack with msgpack's own depth limit (`msgpack_load`, `make_msgpack_unpacker` for the
   streaming path), YAML counts open collections in the y2j event loop; both refuse past 200 (`MAX_JSON_DEPTH`)
   with a 400. NDJSON (and RESTORE) lines go through `json_load`, already guarded. The fuzz target
   `xapiand_fuzz_body` (JSON, YAML, MessagePack, structured around the limit plus byte mutations) found two more
   YAML crashes at once: a bare-scalar document indexed an empty collection stack, and an empty document popped
   an empty value stack. Fixed (a 400 for no document); an error also stops the alias pass. Not probed over HTTP
   before the fix whether the framework reaches the decoder with an empty body.
3. A 150-level field path (`Term too long (> 245)`, an `errc::invalid` engine error) answers 400; any
   `errc::invalid` that is not already a client error does (it was 500). Also turns the `_scale_weight` list
   misuse (`op not valid with a list of subqueries`) from 500 to 400.
4. `_fuzzy` text searched the whole text for a tilde once per word (quadratic). Hoisted: 12,000 words 3.02 s to
   0.04 s on the ASan server, 14,000 4.23 s to 0.05 s, 100,000 timed out to 0.34 s (400).
5. Changelog: queries over 4,096 clauses or nested past the cap are refused with 400, the new chain limit, the
   body depth limits and the crash fixes.
6. Proof: `xapiand_test_engine_query` 22442 checks (`test_chain_cap`: the limit per operator, wrappers, appends,
   nesting through an OR, and the matcher running 256 phrases), `xapiand_test_text` 13224 (`test_body_depth_per_format`:
   200 loads and 201, 400 and 100,000 are refused for MessagePack arrays and maps and YAML flow sequences, flow maps
   and block maps; bare scalars load; empty documents are errors) on ASan+UBSan and release. Over HTTP on the ASan
   server: the re-check's probes (`p7` text, flat, dsl, qstr, body, yaml, mp groups, `p8` width shapes at 650, 1,365
   and 4,095, `p9` MessagePack, `sr3.py`): 682 requests, every answer 200, 204 or 400 except 15 transport-level
   resets on `q=` URLs of 20,000+ characters (as in the re-check) and the aggregation-nesting timeout the review
   already noted; no crash. `harness/query_depth_check.py` (now with the worst shape and one per operator) exit
   0 on both builds. Docs E2E 576/576 (339 requests, uncovered 0). `fuzz_body` 90 s seed 7 and 60 s seed 11 (174k
   bodies), `fuzz_text` 40 s seed 7 (369k texts): no violation.
   Revert checks: chain check off fails `test_chain_cap` (10 checks); msgpack limit off aborts `xapiand_test_text` in
   `msgpack::object` copy (`object.hpp:570`, the review's frame); YAML limit off fails 4 checks; the two YAML fixes off
   SEGV in `countEntry`; the reverted field-path and `_fuzzy` sources answer 500 for 150 levels and 3.02 s for 12,000
   words (100,000 times out), the fixed 400 and 0.04 s. Mutants: FILTER not joining fails 3 checks, OR joining fails
   `test_chain_cap` ("threw ... too many phrases").

Left open: YAML aliases were not probed for amplification (a chain of aliases to anchors copies the value each
time); the aggregation 10-level nesting timeout in the review; the DSL depth margin (cap 45, ASan crash 80).

## Next

Route the findings above (wildcard cap, url_parser, YAML alias amplification). Open for the lead: the `cjk_words` segmenter (decision 2).

## Blocked on

Nothing.
