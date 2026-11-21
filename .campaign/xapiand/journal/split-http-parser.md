# split-http-parser: journal

State: waiting on jobs 261121-115212-41a5fe 261121-115212-e66e17 261121-115215-a5e4a8
Quest: .campaign/xapiand/quests/split-http-parser.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done in the http-parser worktree (3 commits: 7f3b7fd, 799b36e, 738d611). Jobs submitted: build-test 261121-115212-41a5fe, split 261121-115212-e66e17, ctest -N at head 261121-115215-a5e4a8. Baseline ctest -N at acb0aff still to submit (a --quest job supersedes the queued one, so submit it after the head job ends), then diff the two listings.

## Next

1. Submit the ctest -N job at acb0aff (--commit acb0aff --for A3) once 261121-115215-a5e4a8 finishes; diff the listings.
2. Record the evidence table; push the branch.

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

## Result

http_parser.cc (2,850) is 164 lines plus pieces, none over 675: execute states in 4 .inc files, macros/tables/scan .inc, shared headers http_parser_internal.h and http_parser_chars.h, http_parser_api.cc and http_parser_url.cc. test/diff.cc (1,004) is 431 plus 4 .inc. Verified by a sorted-line multiset diff against the originals: nothing lost, only added includes, one-line file comments and repeated license headers.

## Rulings I made

- The parse loop `execute<lenient>()` is one 1,600-line function with `goto` between states, so no .cc split can keep it unchanged under 1,000 lines. Its states (and its macros, tables and bulk scans) are `.inc` files included inside the function in http_parser.cc, one translation unit as before: identical text to the compiler, so inlining (find_crlf, skip_url_chars, parse_url_char, message_needs_eof) and speed cannot change.
- Shared helpers in two headers (`static`, so each .cc has its own copy, as before for execute): `http_parser_internal.h` (method_strings, message_needs_eof, should_keep_alive, SIMD selection) and `http_parser_chars.h` (char classes, normal_url_char, parse_url_char; used by the loop and by http_parser_url.cc). No dynamic initializer moved; `header_char` (constexpr) stays with the loop.
- http_parser_api.cc and http_parser_url.cc join the `http_parser` target; the url and api code do not call the hot loop.
- test/diff.cc: the shared anonymous-namespace state (counters, impls) means a second translation unit would need extern declarations, so its sections are `.inc` files in one TU.
- Order of file-scope statics in the loop's TU changed slightly (tables after the shared header); no semantic effect.
- Left alone, outside my file set, and now stale: `tools/mutants.py` (reads and compiles only http_parser.cc; mutates scan code now in .inc files), the build line in bench/bench.cc, and prose in README.md, ARCHITECTURE.md, AGENTS.md naming http_parser.cc as the single source. For the lead.
