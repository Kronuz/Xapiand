# split-http-parser: journal

State: waiting on jobs 261121-121127-15cda8 (A1) 261121-121127-057585 (split) 261121-121129-b6b2a3 (A6) 261121-121132-8993a2 (A3 head) 261121-121135-172d36 (A3 base)
Quest: .campaign/xapiand/quests/split-http-parser.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Reworked per the lead's ruling (SPLITTING.md), pushed at http-parser 743f2a4. Jobs at 743f2a4 listed in State. When they end: diff the `Test #` lines of the A3 head and base logs; record the Evidence table; answer split_check warnings (tools/split_check.py was missing at the last try).

## Next

1. Read the finished jobs; fill Evidence; set State to ready for review.
2. Tell the lead which tools are stale (see Rulings, last item).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-115212-41a5fe |
| A2 | split 261121-115212-e66e17 |
| A3 | ctest -N at head 261121-115215-a5e4a8; at base acb0aff to come |
| A4 | docs/source-layout.md lists each new file |
| A5 | CI on the pushed branch |
| A6 | selfcontained job 261121-115551-b79709 (the two new .h files; the .inc files are not headers, see Rulings) |

## Result

http_parser.cc (2,850) is 164 lines plus pieces, none over 675: execute states in 4 .inc files, macros/tables/scan .inc, shared headers http_parser_internal.h and http_parser_chars.h, http_parser_api.cc and http_parser_url.cc. test/diff.cc (1,004) is 431 plus 4 .inc. Verified by a sorted-line multiset diff against the originals: nothing lost, only added includes, one-line file comments and repeated license headers.

## Rulings I made

- Parse loop whole: `execute<lenient>()` and the macros, chunk-extension grammar and bulk scans only it uses are in http_parser.cc, verbatim and in place (about 2,000 lines; docs/source-layout.md says why). No `.inc` file remains.
- http_parser_tables.h (token, hex and header-char tables, special header names): a header a maintainer opens alone, self-contained (includes http_parser.h and http_parser_chars.h); only http_parser.cc includes it.
- http_parser_chars.h (char classes, normal_url_char, parse_url_char) and http_parser_internal.h (SIMD selection, method_strings, message_needs_eof, should_keep_alive): shared by the loop and the api/url units, `static`, so every includer has its own copy as before; no function lost `static`, no dynamic initializer moved (header_char is constexpr and moved with the tables to the one TU that uses it).
- http_parser_api.cc and http_parser_url.cc kept as units; neither is hot.
- test/diff.cc kept whole (1,004 lines, byte-identical to base): one differential test of one subject sharing an anonymous namespace's state.
- File-scope statics in the loop's TU keep their order except the tables, now ahead of the macros.
- History: the branch holds the first split (.inc pieces), then the commits that undo it; the final tree is what counts.
- Stale outside my file set: `tools/mutants.py` still compiles http_parser.cc alone: it needs http_parser_api.cc and http_parser_url.cc added to its `base` objects (loop in main(), lines ~201-206), and its two mutants `harden-http10-te-close` and `harden-http10-te-close-version` mutate should_keep_alive, now in http_parser_internal.h, so run_mutant must read that header for them. The other 96 snippets still match http_parser.cc once. bench/bench.cc (build line in its header comment) and the prose in README.md ("compiles one source file"), ARCHITECTURE.md and AGENTS.md (http_parser.cc as the single source) are stale too.
