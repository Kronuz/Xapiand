# restore-free-off-loop: journal

State: ready for review (evidence reruns at the merged head Xapiand:a6f6c9bbc queued: 261116-163129-344a10 261116-163129-4fff0e 261116-163130-b509cb 261116-163130-ab4353 261116-163130-1875e5 261116-163130-006d68 261116-163133-9cc384 261116-163133-2c3c88)
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

## Round 2 (lead, 11-16): the ~8 ms left

- E1 (head, strict beside 4 light holds; .scratch/e1): `GET /` (reads no document) p99 in RESTORE 8.8 ms,
  `GET /accounts/{id}` 9.0 ms: the delay is the loop, not the document read. Spin at 1 ms: 1,207 turns
  over 1 ms in 20 s (1,004 of 1-2 ms, 21 of 2-4 ms, 7 over 4 ms). Stall captures by first app frame: 182
  `take` (`reads_stored` -> `MsgPack::_init_map` builds each object's whole child map on the loop, and
  `run.push_back` with a placeholder MsgPack, an 8 KB zone), 48 `Admitted::~Admitted` (`run.clear()`).
  `sample`: `Admitted::~Admitted` 22.5% of the loop's busy samples (madvise 10.8%), `_init_map` 13.8%.
  Model: a light GET waits one heavy turn per loop pass (task docs/lanes.md); turns of 1-2 ms with no
  checkpoint (the take loop) times the GET's 4-5 passes is the 8 ms.
- E2 (`reads_stored` reads the raw map; `run.clear()` in a chore): spin at 1 ms, 72 turns over 1 ms (head
  1,686 in the same hold). GET p99 unchanged (7.5/10.1 against 7.2/10.5 ms, beside 11 light holds).
- E3 (E2 plus a checkpoint after each taken object): spin 115 turns over 1 ms; its 4 turns of 16-22 ms hold no
  RESTORE frame (captures are the spin probe's own fprintf to node.log). GET p99 unchanged.
- E4/E5 controls: the client's own 1 ms ticker wakes 10-11 ms late at p99.9 in the RESTORE phase (0.5 ms in the
  trivial phase); a GET touching no index (`/:none`, 400) has the same p99 as the document GET (8.3-8.9 ms).
- E6 (cause): the probe ran the RESTORE client in the GET generator's Python loop, and building and sending each
  5,000-document body stalls it 10-20 ms. RESTORE client in its own process: round-1 head 2.25/2.86 ms, round 2
  1.71 ms, master 11.25 ms; A/A trivial 0.87-1.10 ms. Probe fixed (Xapiand:8961d6ae0); round 2 committed
  (Xapiand:386235f70); master merged (Xapiand:a85559290).

## Next

1. Reruns at Xapiand:a6f6c9bbc (master merged again, BASE kept at Xapiand:49a73300d): ABBA 261116-163129-344a10,
   261116-163129-4fff0e; spin 261116-163130-b509cb (now with its own workdir); items 261116-163130-ab4353;
   mutants 261116-163130-1875e5, 261116-163130-006d68; build-test 261116-163133-9cc384; e2e 261116-163133-2c3c88.
2. The spin job at Xapiand:a85559290 (261116-152346-3f7758) exited 0 with no data: workdir = output dir, so the
   node's start removed it, and the verdict passed an empty dir. Fixed in Xapiand:395fe52f2; A1's turn bound
   rests on the rerun b509cb.

## Blocked on

Nothing. msg-49 and msg-50 answered by the lead: tests/test_loop_yield.cc added to the file set;
the delete-count test is committed (Xapiand:6e31d4d65) and jobbed.

## Decisions

- Probe's RESTORE client in its own process (Xapiand:8961d6ae0): lead confirmed, 11-16. It fixes the probe, not
  the server; the bound and the base-versus-head comparison are unchanged; `/:none` and the client ticker show the
  8 ms was the client's own loop.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | At Xapiand:a85559290, exclusive ABBA against master Xapiand:49a73300d: 261116-152341-b40b49 PASS (p99 1.44/1.61 ms, master 10.90/10.67, bound 2.76), 261116-152344-390c05 PASS (1.58/1.66, master 10.11/10.24, bound 2.77). Spin 261116-152346-3f7758 recorded nothing (harness bug, fixed). Reruns at a6f6c9bbc: 261116-163129-344a10, 261116-163129-4fff0e, spin 261116-163130-b509cb |
| A2 | 261116-152341-b40b49 PASS (7,088 docs/s, master 6,686, floor 6,542), 261116-152344-390c05 PASS (7,119, master 6,711, floor 6,576); reruns 261116-163129-344a10, 261116-163129-4fff0e |
| A3 | At a85559290: e2e 261116-152405-fc88a5, build-test 261116-152404-9e4b93, items 261116-152349-798018 (byte-identical), mutants 261116-153503-8232b1 (master's handler fails the results test), 261116-153509-f2b0ec (round-1 handler fails the off-loop test), all passed. Reruns at a6f6c9bbc: 261116-163133-2c3c88, 261116-163133-9cc384, 261116-163130-ab4353, 261116-163130-1875e5, 261116-163130-006d68 |
| A4 | docs/perf/restore-stall.md, "Fix: the results in one buffer" and "Fix, round 2: the 8 ms left" (with the exclusive table) |

## Result

Round 1: DocIndexer's results are one packed MessagePack buffer plus (offset, size) per object
(src/database/handler.{h,cc}). Round 2: the loop no longer builds or frees each object's MsgPack tree (raw
`reads_stored`, the run freed in a chore, a checkpoint per taken object), and the probe runs its RESTORE client in
its own process (the round-1 8 ms was mostly the client's own stall). Exclusive ABBA: p99 1.44-1.66 ms against
master 10.1-10.9 ms (bound 2.76-2.77), 7,088-7,119 against 6,686-6,711 docs/s. Note: docs/perf/restore-stall.md.

## Rulings I made

- Round 2: the probe runs its RESTORE client in a process of its own. Its in-process client stalled the GET
  generator's loop 10-20 ms per chunk, so the 8 ms A1 failed on was mostly the probe's own (E4-E6). The criterion,
  the verdict and its 2x bound are unchanged; the old A/B rows are measurements of the old probe.
- Round 2 also takes the per-object MsgPack trees off the loop (raw `reads_stored`, the run freed in a chore, a
  checkpoint per taken object): turns over 1 ms 1,686 -> 72-115 per 20 s RESTORE; p99 2.25-2.86 -> 1.71 ms in
  screening. Not slicing: the loop no longer builds or frees the trees at all.
- The objects-off-loop test bounds the loop's frees at 50 per object (34 measured, 75 on round 1).

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
