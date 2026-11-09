# perf-watch-peek: journal

State: working
Quest: .campaign/xapiand/quests/perf-watch-peek.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Building and testing E2 locally (http worktree `build-dev`), then the A/B jobs.

## Next

1. Commit E2 and bench/watch_ab.sh; `campaign verify perf-watch-peek` (build-test, A2).
2. Light job `watch_ab.sh build` (slot pwp-ab), strict smoke/screen (base, E1, E2), then the exclusive confirm (base against head, A/A) for A1.
3. Write docs/perf/watch-peek.md with the numbers (A3); A4 is docs/mechanisms/coroutine-handlers.md.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|

## Attribution (before, http main 66050ec, i9-9980HK, counting build, local probe, not evidence)

The quest's "extra read" is CancelWatch::engage: `httpc` (a handler that asks for its token, as
Xapiand's SearchService::handle does on every request) against `http` (never asks):
allocs/req 6.00 against 3.00, Asio handlers/req 3.00 against 1.00. Read from Asio 1.36
(`kqueue_reactor::start_op`, `epoll_reactor::start_op`): async_read_some tries the recv at once
(speculative), so engage costs a MSG_PEEK recv (EAGAIN) plus the armed read's speculative recv
(EAGAIN) before the data recv: three recv per asking request against two. The next request's
read then goes through `read_held` and `drain` (two task frames) and the `done_` channel (one
op, one more handler).

Experiments (http branch quest/xapiand/perf-watch-peek):
- E1 http:f43dee4: engage reads without waiting into the input buffer (decides inline as the peek
  did), and the armed read on plain TCP is `async_wait(wait_read)` then a recv that does not wait:
  -1 recv per asking request.
- E2: the pending read resumes the next reader directly (task's await_asio rule: in place in a
  top-level handler on its loop, else posted); no read_held/drain frames, no channel.

## Result

<pending the A/B jobs>

## Rulings I made

- R1 Retargeting confirmed from code: "the keep-alive watch byte's extra read" is CancelWatch's
  peek and the armed read behind it (quest Decisions, first bullet).
- R2 The [L] check "engage() with data waiting ... reads nothing" pinned the peek, not behavior:
  it now checks that engage reads ahead kReadAhead bytes, ends the watch, fires nothing, and the
  stream stays intact and in order. The 4096-byte bound is unchanged.
- R3 `httpc` kind added to bench/server_compare.cc so the cost is measured where it is paid.
