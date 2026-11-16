# restore-free-off-loop: journal

State: working
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

1. Jobs: build 261116-082820-01ef20 (light, slot rfo-ab, head + base with lag patch), then strict
   261116-082828-7f33b2 and 261116-082830-81632d (ABBA, A1 p99 + A2) and 261116-082832-e7e5ad
   (spin, A1 turns); verify build-test 261116-082744-47495e, e2e 261116-082744-9228f8 (at fb633e0,
   re-verify at final head). Combined verdict: `harness/restore_stall_verdict.py` over the slot's
   .scratch/rs-abba1 rs-abba2 rs-spin.
2. Local: .scratch/build test_loop_yield run (restore item tests).
3. Fill the note's Fix section with the numbers (A4).

## Blocked on

msg-49 to the lead: the deterministic test (thread-local operator-delete count across
~DocIndexer: >= N on master, a handful with the fix) needs tests/test_loop_yield.cc, outside the
file set. Default if unanswered: no test, the reason recorded.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
