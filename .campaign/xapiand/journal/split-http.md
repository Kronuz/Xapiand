# split-http: journal

State: waiting on jobs 261121-145036-b836fa (A2, split_check from split-check-sources at 944e8cf)
Quest: .campaign/xapiand/quests/split-http.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Reassessed against cpp-libraries docs/SPLITTING.md. Branch pushed at http:944e8cf (repo Kronuz/http). Commits: http:8c6cd46,
http:3d931dd, http:b283dcd (first round), then http:7de3a54 (http_asio/ merged to four pieces), http:4009a43 and
http:944e8cf (tests reverted to moves-only). Jobs queued at 944e8cf: build-test b632ee (A1), A3 ae1958 (head vs base via
--ref, diffed), A6 4665eb (selfcontained). The `split` check waits for split-check to land (split_check.py is absent).

## Next

1. Read A2 job 261121-145036-b836fa; answer each warning (using-declarations, dropped static, prototypes: see Rulings); fix any real error as moves only.
2. After split-check lands the lead reruns `campaign verify split-http`.
3. A7: the lead reads the split.

## Blocked on

Nothing. Main merge: already up to date (base e4ad5b9). A1/A3/A6 jobs passed at 944e8cf, no rerun needed. Branch already pushed.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-121235-b632ee at 944e8cf (f75201 at b283dcd cancelled, superseded) |
| A2 | 261121-145036-b836fa (fixed checker at 944e8cf; replaces cancelled 121236-b52a0a and failed 123002-407242, which ran the old checker). Warnings to answer when it finishes |
| A3 | 261121-121244-ae1958 at 944e8cf vs base e4ad5b9. Earlier first-round ctest -N jobs, same list shape: 261121-114853-b2726f (b283dcd), 261121-114903-8148a4 (3d931dd), 261121-115542-54b74a (8c6cd46) |
| A4 | docs/source-layout.md |
| A6 | 261121-121251-4665eb at 944e8cf. Failed run examined: 261121-115533-838bff (asio.hpp not on the include path; the command now passes -I and -DASIO_STANDALONE) |
| A7 | the lead reads the split |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- http_asio.h: four pieces, units a maintainer opens alone: response.h (shared includes, configuration, AsioResponseWriter), connection_io.h
  (write helpers and ConnectionInput), connection.h (the session's shared state and serve_connection), service.h (HttpAsioService).
  The first-round options.h (92 lines) and connection_state.h (162) joined neighbors. The repo is flat http_*.h headers, so http_asio/ is a directory
  named after the header. http_asio.h keeps the licence and overview comment and includes the pieces; response.h carries the include block, and each
  piece includes the previous one, so each compiles alone (A6).
- Added lines only: `#pragma once`, a one-line comment, the include of the previous piece, `namespace http {` / `namespace detail {` openers and closers
  (the original `namespace detail {` spans connection_io.h and connection.h).
- Tests: NO section wraps remain. The first-round wraps (sections of main as functions, http:3d931dd, http:b283dcd) are undone by http:4009a43 and
  http:944e8cf: a statement moved out of main is not a move (SPLITTING.md, split_check rule 3). main() and its sections stay whole in coroutine_test.cc
  (about 770 lines, twelve sections sharing one application) and readahead_test.cc (about 430 lines). If the lead wants the sections as functions in
  topic files, that is a reviewed non-move; the first-round commits show it works.
- Moved out as pure moves: test/coroutine/support.h (client, Obs, wrapper_scheduler, in `detail`), app.h (App), body_stream.cc (body_stream_tests);
  test/readahead/support.h (client, App, Server, Held) and input_tests.cc (the [U] sections, one file with its original #ifndef pair, about 600 lines,
  one topic kept whole).
- Linkage changes: `g_fail` is `inline int` (one counter across the translation units; was `static int`); `body_stream_tests`, `input_tests` and
  `read_ahead_tests` lose `static` (prototypes added in app.h's includer coroutine_test.cc and support.h). The helper functions in the headers keep
  `static`. No namespace-scope object with a dynamic initializer moved. Test headers keep their included `static` functions per translation unit.
- Inlining: tests and a header-only library, so no Release size comparison applies.
- CMake: the extra .cc files are added to the existing coroutine_test and readahead_test targets; test names unchanged.
- Test headers (support.h, app.h) chain includes; selfcontained.sh skips test directories, so they are self-contained by construction only.
- Files kept whole above 1,000 lines: none.
