# consensus-rpc-lane: journal

State: CI fixes at consensus e1f249d (CI run 44766862421 failed calls on FreeBSD, group-full on macOS); waiting on jobs 261120-082541-e38907 (build-test), 261120-082602-299054 (rpc-shared-credit control at mutant 0a00754); pushed for CI
Quest: .campaign/xapiand/quests/consensus-rpc-lane.md (the lead's; never edit it)

## Doing

Correctness review (`.campaign/xapiand/reviews/consensus-rpc-lane-correctness.md`, CHANGES at
1cd353f) folded in at consensus e7d615c (code) and f748f79 (rpc-lane.md):

- F1: the reported source is the value of `Opened::groups` (`GroupTable<NodeId>`); the second
  table and `Empty` are gone. Idle, hosts 5 loops 1, 3 local runs: 316.2 B per idle group
  (budget 372; main 283.6, 1cd353f 373.3). The +32.6 B over main is the 8 B value in each of
  4 connections' entries; no FAIL in `consensus_transport_idle`.
- F3 (lead's ruling): the constructor refuses `maximum_frame` < `rpc::frame_bytes` (65,584) when
  the lane is on; `hello_test version` refuses 32 KiB with 16 KiB rpc_bytes.
- F7: a throw from accept, `body`, `credit` or `done` fails that call alone (server `Error
  handler_failed`, caller `Cancel cancelled`, text = what()), counted in `Lane::failed()`;
  `accept_rpc` erases its handle first. `rpc_host_test local-throw`: both calls end code 6, links
  2 then 2, malformed 0, group frames 2 of 2, next 1 MiB echo exact, pools 0.
- F4/T7: `late_extension()` in deadline.h is the rule; `late_deadline` and the call deadlines
  (Lane via `LaneOptions::late`, and calls waiting to dial) use it. `rpc_test late-deadline`.
- F10 pools declared before `loops_`; F12 A3's control is `rpc-past-credit`; F13 encode first.
- F12 follow-up 22b979e: at f748f79 rpc-past-credit passed `stream` (job 261120-073257-1c9ed0,
  exit 0: an instant consumer regrants ahead of every chunk, so rule 7.6 never fires). `stream`
  now checks the sender's queued bytes (`outgoing` high water) within one window: 4,194,304 B
  at base, 4 of 4 runs. At f748f79 the other five controls failed as intended (exit 8, 8, 134,
  8, 8: 261120-073255-4083da, 073259-715956, 073303-5d2242, 073306-214cf2, 073309-5b931e);
  all six rerun at 22b979e.
- Local `quiet light` runs at e7d615c: rpc, rpc_host 6/6, hello 3/3, both fuzz replays, idle pass.

- CI fixes e1f249d (test only): `calls` on FreeBSD: the caller's 300 ms timer fired 14 ms late,
  T7 extended it once, and the server's Error arrived first (caller outcome error, not cancel);
  both are correct, so the check compares the lane's counters with each call's seen outcome.
  `group-full` on macOS: 11 and 14 calls in a 1.5 s window, set by the runners' sleep_for
  granularity; now 15 calls, with the stalled group's frames still undelivered at the last.
  Local light runs 5/5 each (caller cancel, ended ok 1 cancel 2, served 3; 15 of 15, delivered 0).

## Next

1. Read the seven jobs in State; fold any failure.
2. When done (the lead's): push for CI (asan, tsan), the security review, the `--later` jobs
   (fuzz hours, 1,000 streamed calls on macOS, the 4 GiB strict job).

## Blocked on

1. The lead, file set: `docs/mechanisms/host-transport.md` (Tables, lines 155 and 160) still says
   "A set is `GroupTable<Empty>`, 8 B an entry" and `Opened::groups` "the group (8 B)"; after F1
   it is 16 B (the group and the last reported source). Outside the set; not edited.
   Recommendation: add it to the set, or fix the two lines on landing.
2. The lead: `docs/design/rpc-lane.md:290` (the 4 GiB job) names control `rpc-one-frame`, which
   is now `rpc-past-credit` (F12). The design is a spec of bytes; recommendation: update the name.
3. F5 (`lane_idle`/`Drain` with `hold(node)`): not small, not built. It needs a per-peer idle
   timer and a hold count, `drain()` then a graceful close only of lane-only Bulk links (Bulk
   also carries group frames), Links' peer set gaining lane demand, the peer's close on `Drain`,
   and A3's "second call in flight at `lane_idle`" test. Recommendation: `cluster`, per the review.
4. Nits left: F6 (DTHL0003 rename: magic, corpus seeds, node_hello bytes; R13 keeps it), F8
   (`protocol_not_supported` on `WireVersionRefused` needs the dial's failure reason), F9
   (alternate inline Calls), F11 (`cluster` wires `on_peer_limits`). None is a one-liner.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `rpc_host_test calls`, `group-full`, `call-full`, `local-throw` (F7) and `rpc_test` (incl. `late-deadline`, T7) in build-test 261120-074701-268a9a at 22b979e (pending); controls `--fails-without`: rpc-shared-credit (mutant 858f25f) 261120-074650-d657bc, late-deadline (a1e77b5) 261120-074656-03dba7, local-throw-closes (6b14807) 261120-074659-a5a3bd. Superseded: 261120-063245-5d4c86 (failed: idle, F1) and 063253-b0630d (build failed, F2) |
| A2 | `rpc_host_test lane-only` (262,144 B exact, links 2, group frames 0) in build-test 261120-074701-268a9a (pending) |
| A3 | `rpc_host_test stream` (32 MiB under credit; heartbeat gaps < 1,000 ms) in 261120-085347-8daf82 (passed at e1f249d8d); control rpc-past-credit (mutant 7caaa5a, F12) `--fails-without` 261120-074652-9c5498 |
| A4 | `hello_test leader`, `codec`, `version` (now also refuses a 32 KiB frame below the lane's 65,584, F3) in 261120-085349-81eac6 (passed at e1f249d8d); controls `--fails-without`: hello-limits-ignored (mutant d6967cb) 261120-074654-e15f02, lane-frame-unchecked (24da848) 261120-074657-36a217 |
| A5 | [artifact] consensus `docs/mechanisms/rpc-lane.md` (On Bulk: Deadlines, Frame bound; Evidence: local-throw, controls), `docs/mechanisms/hello.md` at 22b979e |

## Result

Revision 2: the one spec of the lane's bytes, each decision marked [transport] (consensus-transport
A7) or [rpc-lane]. Six kinds (Drain added), the full validity list (D3), atomic renewal of credit,
a sender pool, separate serving and calling pools, T1-T8. This quest changes no byte.

## Rulings I made

- R17 (review F1) One per-connection table: the last reported source is the `Opened::groups`
  value; an open again on the same connection keeps it, a mismatched open erases it.
- R18 (review F7) Local throws fail their call alone, including `done`'s (counted; the call has
  ended). A Serve handler that acts outside its events still fails (documented contract); now as
  `handler_failed`, not a closed connection.
- R19 (review F4, T7) The late rule applies once per waiting place: a call that waited for its
  dial and then for its deadline on the lane can be extended in each (each capped at its timeout).
- R20 (review F12) A3's control renamed `rpc-past-credit`: `sendable` returns a full chunk past
  credit, so the receiver's rule 7.6 aborts and `stream` fails on its outcome.
- R21 Review-fix controls carry the nearest criterion: late-deadline and local-throw-closes A1,
  lane-frame-unchecked A4. Each mutant builds in its own `build-ctl-<name>` with Ninja (F2).

- R10 (F3) The transport's lane wiring (design T2, T3, T5, D7) had no builder after D123 dropped
  consensus-transport; built here, since A1-A3 need it. Not built: `lane_idle`/`Drain` on idle
  or shutdown (the transport keeps connections until `stop()`), `hold(node)`; T7 built later
  (R19); docs/mechanisms/rpc-lane.md says so.
- R11 A call with no Bulk link waits and dials; a failed dial retries every `redial` until the
  call's deadline (`timed_out`), never `not_connected` (a lost tie-break is a failed dial).
- R12 A chunk's `rpc_outgoing_bytes` charge returns when the connection takes it; the connection
  takes a feed only once its queue is out, so the bound holds within one feed.
- R13 DTHL0002 keeps its magic and grows to 152 bytes (formats open until release).
- R14 The host refuses a `maximum_frame` below its own rpc_bytes' frame; not a group window
  below it: a frame always goes when nothing is outstanding, and existing tests use small windows.
- R15 PeerLimits are reported per (connection, group, source incarnation) before the source's
  first admitted frame; the transport cannot enumerate a node's incarnations.
- R16 A1's control is `rpc-shared-credit` (lane frames wait for the groups' credit, design T2's
  rejected shape); A3's is `rpc-one-frame`; A4's is `hello-limits-ignored`.

- R17 Which end of a deadlined call ends first is a race T7 allows; tests count outcomes as seen,
  never assume the caller's timer wins. Call-count floors are fixed counts, not time windows.

- R1 Lane on Bulk, not Control or a third connection (design D1).
- R2 "End" is a flag on Call/Body, not a sixth kind: one way to end a stream, one-frame calls.
- R3 Call ids by parity of the connection role (dialer odd), no direction bit.
- R4 Per-call windows only; no lane-wide connection window (the grantor owns the memory).
- R5 Overloaded calls are answered `Error overloaded`, never queued nor closing the connection.
- R6 The model lives beside the design as docs/design/rpc-lane-model.py (a design artifact, not
  consensus sources); the lead asked for one new file, the skill asks for an executable model.
- R7 (rev 2) Two receive pools, rpc_serving_bytes and rpc_calling_bytes, so served requests cannot
  take the bytes a host's own calls need (review F8's hold-and-wait).
- R8 (rev 2) Call.window must be nonzero; Drain's call field may be 0 (none accepted).
- R9 (rev 2) The model's rules run as three jobs; all three together outgrow a Python run.
- Decoder corpus: 2 stale 64-byte hello seeds replaced by seed.cc's 152-byte ones (382 seeds before and after); replay clean, hello round-trips (rpc-lane:83792cf).

## Review findings (consensus-rpc-lane-correctness)

| ID | Done |
|---|---|
| F1 | fixed e7d615c; idle 316.2 B (budget 372); build-test 261120-074701-268a9a |
| F2 | resubmitted with own build dirs: 261120-074650-d657bc, 074652-9c5498, 074654-e15f02 |
| F3 | fixed e7d615c (lead's ruling a); `hello_test version`; control 261120-074657-36a217 |
| F4 | T7 built e7d615c; `rpc_test late-deadline`; control 261120-074656-03dba7 |
| F5 | not built: not small (Blocked on 3) |
| F6 | kept (R13); listed |
| F7 | fixed e7d615c; `rpc_host_test local-throw`; control 261120-074659-a5a3bd |
| F8, F9, F11 | listed (Blocked on 4) |
| F10 | fixed e7d615c |
| F12 | control replaced: rpc-past-credit 261120-074652-9c5498 |
| F13 | fixed e7d615c |
