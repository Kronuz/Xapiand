# split-http: journal

State: waiting on jobs 261121-114840-f75201 261121-115542-54b74a 261121-115533-838bff
Quest: .campaign/xapiand/quests/split-http.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done and committed on quest/xapiand/split-http (repo Kronuz/http, worktree http-wt/xapiand/split-http):
http:8c6cd46 http_asio.h, http:3d931dd coroutine_test.cc, http:b283dcd readahead_test.cc (docs/source-layout.md grows
with each). Jobs: build-test f75201 (A1), ctest -N at b283dcd b2726f and 3d931dd 8148a4, 8c6cd46 54b74a,
selfcontained (A6) 838bff. The A3 job at base e4ad5b94a41f must run after the others (a new quest job cancels
queued ones). The `split` check waits for split-check to land (tools/split_check.py does not exist yet).

## Next

1. When 54b74a is done, submit the A3 job at e4ad5b94a41f (same command), diff the four `Test #` lists.
2. After split-check lands (`campaign status split-check`): `campaign verify split-http` for the `split` check; answer its warnings here.
3. Push the branch once jobs are green.

## Blocked on

Nothing; the split check waits on quest split-check.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-114840-f75201 at b283dcd |
| A2 | split check pending split-check landing |
| A3 | ctest -N jobs 261121-114853-b2726f (b283dcd), 261121-114903-8148a4 (3d931dd), 261121-115542-54b74a (8c6cd46), base at e4ad5b94a41f pending |
| A4 | docs/source-layout.md |
| A6 | selfcontained 261121-115533-838bff |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Pieces: http_asio/ directory (the repo is flat headers http_*.h, so http_asio.h + http_asio/ keeps users' include).
  http_asio.h keeps the licence and overview comment and includes the six pieces; http_asio/options.h carries the
  include block (moved from http_asio.h) so each piece is self-contained by including the previous one (A6).
- Added lines (not moves): `#pragma once`, a one-line comment, the include of the previous piece, and namespace
  openers/closers (`namespace http {`, `namespace detail {` and their closers) per piece; the original `namespace detail {` was
  one block across connection_io.h, connection_state.h and serve_connection.h.
- Tests: the sections of main() become functions `section_X(App&)` (coroutine) and `section_X()` (readahead) wrapping the
  original blocks unchanged (comment and block lines verbatim), called by main in the original order; `body_stream_tests`,
  `affinity_death_test`, `lazy_watch_tests`, `held_read_tests`, `lane_tests`, `read_ahead_tests`, `input_tests` lose `static`
  (declared in sections.h / input.h). The affinity death tests stay first in main (forked before any thread).
- Linkage: shared helpers in test/*/support.h and input.h (`check`, `since_ms`, `header_value`, `get`, `post`, `one`,
  `wait_listen`, `spin`, `pattern`, `pending_bytes`, `read_once`) went from `static` to `inline` in `namespace detail`
  (with `using namespace detail;`); `g_fail` is now `inline int` (one counter across the TUs); `until` is a plain template.
  Server/Held/App/Obs/Conn types moved into detail unchanged. No dynamic-initialized namespace-scope objects were moved.
- CMake: the extra .cc files are added to the existing coroutine_test and readahead_test targets; test names unchanged.
- Test headers (support.h, app.h, sections.h, input.h) chain includes; selfcontained.sh skips test dirs, so they are self-contained by construction only.
