# perf-watch-peek: journal

State: ready for review
Quest: .campaign/xapiand/quests/perf-watch-peek.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done: both timing jobs read, docs/perf/watch-peek.md Timing filled and the CHANGELOG entry
committed (http:51d153a, docs only; build-test 261109-145852-514f5a carries across it).

## Next

1. Correctness review (lead launches). Xapiand's pin of the landed http commit is the lead's.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | exclusive confirm 261109-150350-f39012 (http:13933f0 vs base http:8262172 and E1 http:f43dee4, A/A copy; 6 rounds x 4 s); strict screen 261109-144804-ff5f5b agrees |
| A2 | 261109-145852-514f5a (build-test at 13933f0); mutants 261109-150416-197873; fails-without 261109-145905-5a00d5 |
| A3 | `docs/perf/watch-peek.md` (http:51d153a) |
| A4 | `docs/mechanisms/coroutine-handlers.md` (http:6ec7324) |

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
- E2 http:6ec7324: the pending read resumes the next reader directly (task's await_asio rule: in place in a
  top-level handler on its loop, else posted); no read_held/drain frames, no channel.

## Result

Asking request (`httpc`), after vs before, median per-round ratio (range) against A/A range:
1 conn 1.39x faster (1.37-1.98 vs A/A 1.00-1.05), CPU 24.4 -> 18.8 us/req (21% less);
8 conns 1.10x faster (1.06-1.18 vs A/A 0.95-1.08; low end touches A/A), CPU 17.4 -> 15.3 us;
64 conns noise (0.82-1.11 vs 0.91-1.22). Non-asking `http`: noise at 1/8/64. E1 alone: noise
everywhere (0.99-1.02); E2 (no frames, no channel) carries the gain. Asking's extra CPU over a
non-asking request: 5.5/3.1/1.3 us before, 0.7/1.1/0.7 us after (residual not attributed).
Handlers/req 3 -> 1, allocs/req 6 -> 3 (counting build). Write-up: http docs/perf/watch-peek.md,
CHANGELOG.md 2026-11-09 entry.

## Rulings I made

- R1 Retargeting confirmed from code: "the keep-alive watch byte's extra read" is CancelWatch's
  peek and the armed read behind it (quest Decisions, first bullet).
- R2 The [L] check "engage() with data waiting ... reads nothing" pinned the peek, not behavior:
  it now checks that engage reads ahead kReadAhead bytes, ends the watch, fires nothing, and the
  stream stays intact and in order. The 4096-byte bound is unchanged.
- R3 `httpc` kind added to bench/server_compare.cc so the cost is measured where it is paid.
- R4 handler_count (f) asserted that an asking handler costs more handlers (`asks > raw`); with
  E2 it costs none more (1.000 = 1.000, job 261109-144734-7f8a70 failed on exactly that), so the
  check is now equality, and job 261109-145905-5a00d5 shows it fails against base's header.

## Job history

- 261109-144734-7f8a70 build-test at 90cf235: failed, handler_count (f) only (see R4).
- 261109-144744-19e463 mutants at 90cf235: all five killed (no-resume, no-cancel, engage-lazy,
  engage-data, eof-as-data); rerun at the final head.
- 261109-144745-8e7df2 A/B builds (slot pwp-ab): ok.
- 261109-145852-514f5a build-test at 13933f0: ok (handler_count /asks 1.000 = raw).
- 261109-145905-5a00d5 handler_count with base's http_asio.h: fails as it must (/asks 3.000,
  /stream-asks 9.000 against raw 1.000, streamed 7.000).
- 261109-150349-93e938 A/B builds at head (slot pwp-ab): ok.
- 261109-150416-197873 mutants at 13933f0: all five killed.
- 261109-144804-ff5f5b strict screen at 90cf235: httpc 1.40x at 1 conn, 1.09x at 8.
- 261109-150350-f39012 exclusive confirm at 13933f0: numbers in Result.

## Messages

- Lead (CHANGELOG in the file set; docs-only commits keep evidence once the tool fix lands; no
  timing rerun for a docs change): entry drafted in http CHANGELOG.md, uncommitted with the perf
  doc; no timing job resubmitted.
