# split-http-parser: journal

State: waiting on jobs 261121-115542-b293ac 261121-115606-c93564 (then submit the base ctest -N at acb0aff)
Quest: .campaign/xapiand/quests/split-http-parser.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and pushed at http-parser a45cda3 (commits 7f3b7fd, 799b36e, 738d611, a45cda3). Jobs at a45cda3: build-test 261121-115542-b293ac (A1), ctest -N 261121-115606-c93564 (A3 head), selfcontained 261121-115551-b79709 (A6, passed: http_parser_chars.h and http_parser_internal.h ok). Split check 261121-115543-836a81 failed: tools/split_check.py is missing (split-check not landed); resubmit with `campaign verify split-http-parser` after it lands. An earlier ctest -N at 738d611 (261121-115215-a5e4a8) listed 4 tests.

## Next

1. When 261121-115606-c93564 ends, submit the base listing: `campaign job submit --quest split-http-parser --for A3 --level light --minutes 10 --commit acb0aff -- sh -c 'cmake -G Ninja -S . -B build-n >/dev/null && ctest -N --test-dir build-n'`; diff the `Test #` lines.
2. After split-check lands: `campaign verify split-http-parser`; answer its warnings.
3. Fill the Evidence table.

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

- The parse loop `execute<lenient>()` is one 1,600-line function with `goto` between states, so no .cc split can keep it unchanged under 1,000 lines. Its states (and its macros, tables and bulk scans) are `.inc` files included inside the function in http_parser.cc, one translation unit as before: identical text to the compiler, so inlining (find_crlf, skip_url_chars, parse_url_char, message_needs_eof) and speed cannot change.
- Shared helpers in two headers (`static`, so each .cc has its own copy, as before for execute): `http_parser_internal.h` (method_strings, message_needs_eof, should_keep_alive, SIMD selection) and `http_parser_chars.h` (char classes, normal_url_char, parse_url_char; used by the loop and by http_parser_url.cc). No dynamic initializer moved; `header_char` (constexpr) stays with the loop.
- http_parser_api.cc and http_parser_url.cc join the `http_parser` target; the url and api code do not call the hot loop.
- test/diff.cc: the shared anonymous-namespace state (counters, impls) means a second translation unit would need extern declarations, so its sections are `.inc` files in one TU.
- Order of file-scope statics in the loop's TU changed slightly (tables after the shared header); no semantic effect.
- Left alone, outside my file set, and now stale: `tools/mutants.py` (reads and compiles only http_parser.cc; mutates scan code now in .inc files), the build line in bench/bench.cc, and prose in README.md, ARCHITECTURE.md, AGENTS.md naming http_parser.cc as the single source. For the lead.
- A6: the new real headers are http_parser_internal.h and http_parser_chars.h (the latter includes the former: a45cda3). The `.inc` files are textual pieces of one function (loop states, macros, tables, scans) and of main() in test/diff.cc: they cannot compile alone, by design, and are not headers; selfcontained.sh covers *.h only. Lead: if A6 is meant to cover them, the parse loop cannot be split without rewriting it.
