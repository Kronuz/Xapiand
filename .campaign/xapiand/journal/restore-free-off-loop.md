# restore-free-off-loop: journal

State: waiting on jobs 261116-084546-66f6bc 261116-084548-54a3f4 261116-084550-4e8f42 261116-084554-997d0d
Quest: .campaign/xapiand/quests/restore-free-off-loop.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Cause: every `MsgPack` owns a `msgpack::zone` that mallocs an 8,200-byte chunk eagerly (msgpack
cpp11_zone.hpp, MSGPACK_ZONE_CHUNK_SIZE 8192); `_results` held one MsgPack per object (plus a
`MsgPack::MAP()` temp per admit and a `MsgPack result` temp per record), so a 5,000-object chunk
ended with 5,000 8 KB frees in `~DocIndexer`, each a Darwin `madvise`.
Fix (at the allocation): `_results` is now one packed MessagePack buffer (`_packed`) plus
(offset, size) per object (`_at`); `results()` builds the response array once. No per-object
MsgPack exists for results. Compiling locally (.scratch/build) to check, then build-test/e2e jobs
and the strict A/B (harness/restore_stall_build.sh + restore_stall_ab.sh, verdict.py).

## Next

1. Jobs at 11252fa (slot rfo-ab, binaries built by 261116-084544-274492, done): strict
   261116-084546-66f6bc, 261116-084548-54a3f4 (ABBA, A1 p99 + A2), 261116-084550-4e8f42 (spin, A1
   turns); light 261116-084552-9a4f9d (items byte-identical, A3); build-test 261116-084554-997d0d;
   e2e 261116-084554-c40c01 done exit 0. Combined verdict over the slot's .scratch/rs-abba1,
   rs-abba2, rs-spin: `python3 harness/restore_stall_verdict.py DIR...`.
2. Put the A/B table in the note's Fix section (the "is pending; its table goes here" line of
   docs/perf/restore-stall.md) and update its Limits and Reproduce, commit (A4).
3. If the lead widens the set (msg-49/50): commit the test from .scratch/test_loop_yield.draft.cc,
   job it, plus a --fails-without job with master's handler.{h,cc}.

## Blocked on

msg-49/msg-50 to the lead: the deterministic test needs tests/test_loop_yield.cc, outside the file
set. Drafted and run locally both ways (.scratch/test_loop_yield.draft.cc): the indexer of 2,000
objects frees 4 blocks on head, 6,003 with master's _results (CHECK_LT 200 fails). Default if
unanswered: not in the branch, the reason recorded.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261116-084546-66f6bc, 261116-084548-54a3f4 (ABBA p99), 261116-084550-4e8f42 (spin turns): queued |
| A2 | 261116-084546-66f6bc, 261116-084548-54a3f4: queued |
| A3 | 261116-084552-9a4f9d (64 of 64 RESTORE responses byte-identical, base vs head); e2e 261116-084554-c40c01 passed; build-test 261116-084554-997d0d running; test_loop_yield and test_node passed locally |
| A4 | docs/perf/restore-stall.md, "Fix: the results in one buffer" (A/B table pending) |

## Result

DocIndexer's results are one packed MessagePack buffer plus (offset, size) per object
(src/database/handler.{h,cc}); results() builds the response array once. Before, each result was a
MsgPack whose zone mallocs 8 KB eagerly: 5,000 x 8 KB freed in ~DocIndexer's one turn. Responses
byte-identical (A3 job). Timing numbers pending the strict jobs; note docs/perf/restore-stall.md.
Leaves open: the msgpack library's eager 8 KB first chunk costs every MsgPack value everywhere
(input objects, Admitted); a lazy or small first chunk in Kronuz/msgpack is the general fix.

## Rulings I made

- Fixed at the allocation (one packed buffer), not off the loop or by slicing: the cost was 5,000
  8 KB zones, so removing them also removes the per-object zone made and freed while recording.
- Each result value is packed from the same msgpack::object the MsgPack held
  (`pk.pack(msgpack::object(v))`), so the bytes match; checked by harness/restore_stall_items.py
  (64 responses, JSON and MessagePack, base vs head) besides the e2e and test_loop_yield.
- The probe carries its own phase runner (reactor_offload_probe.py is not on master and not in the
  set); lag_floor.cc, fd_poll_cost.cc and run/run2/floor/scrape scripts stay on explore/restore-stall
  (answer 2 belongs to metrics-fd-count).
- The A/B builds head and base in one slot sharing .deps (same pins), both with the lag patch.
- A2's band: base's min minus base's spread (verdict.py); strict holds cap at 10 min, so two ABBA
  rounds plus a spin run, one job each.
