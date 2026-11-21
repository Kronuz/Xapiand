# split-http-parser: journal

State: waiting on jobs 261121-145054-4defec (split, fixed checker at a3180d7); main merged (origin/main 6bc8182, ci.yml only)
Quest: .campaign/xapiand/quests/split-http-parser.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pushed http-parser db201cd: mutants.py, bench.cc, README/ARCHITECTURE/AGENTS follow the split (mutants.py copies the library into each mutant dir, mutates the file holding the snippet among http_parser.cc and the three headers, compiles the three .cc). A3 listings equal at 743f2a4 vs base (4 tests: http_parser, http_parser_diff, http_parser_fuzz, http_parser_conformance; jobs 261121-121132-8993a2 and 261121-121135-172d36); head job at db201cd 261121-121503-33e747 reruns it.

## Next

1. Read the split job; warnings answered under Rulings. Earlier: the mutants job (one mutant in http_parser.cc: url-no-budget, one in http_parser_internal.h: harden-http10-te-close) must report KILLED for both.
2. After split-check lands, `campaign verify split-http-parser`; fill Evidence; ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-121452-92646b, mutants 261121-121453-cb7868 (db201cd) |
| A2 | split 261121-145054-4defec (fixed checker, a3180d7); older 261121-123002-909142 failed on the old checker |
| A3 | ctest -N 261121-121503-33e747 (db201cd), base 261121-121135-172d36 |
| A4 | docs/source-layout.md lists each new file |
| A5 | CI on the pushed branch |
| A6 | selfcontained 261121-121501-c2f765 (db201cd) |
| A7 | lead's [manual] read |

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
- (Fixed in db201cd, now in my file set:) `tools/mutants.py` still compiles http_parser.cc alone: it needs http_parser_api.cc and http_parser_url.cc added to its `base` objects (loop in main(), lines ~201-206), and its two mutants `harden-http10-te-close` and `harden-http10-te-close-version` mutate should_keep_alive, now in http_parser_internal.h, so run_mutant must read that header for them. The other 96 snippets still match http_parser.cc once. bench/bench.cc (build line in its header comment) and the prose in README.md ("compiles one source file"), ARCHITECTURE.md and AGENTS.md (http_parser.cc as the single source) are stale too.
