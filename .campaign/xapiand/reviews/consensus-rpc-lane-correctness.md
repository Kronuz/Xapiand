# consensus-rpc-lane review: correctness

Commit: consensus:1cd353f
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Lens: correctness, carrying the lead's concurrency and wire questions. The design is sound. Credit is
separate from the groups', there is no hold-and-wait between the pools, a Lane dies on its own loop,
and the hello refuses bad input. Two things block landing. The suite is red: this branch puts the
`consensus_transport_idle` budget over its limit. The three `--fails-without` controls were also never
recorded. The other findings do not block. The main one: the constructor accepts a frame bound
below the lane's own chunk (F3).

## Evidence I relied on

- Suite 261120-063245-5d4c86 at 1cd353f, **failed (exit 8)**. 77 of 78 tests pass, including all five
  `rpc_host_*` and all three `hello_*` cases, `node_hello`, `rpc` and the fuzz replays.
  `consensus_transport_idle` fails: "an idle group within 164 + 52P bytes (hosts 5, loops 1)".
- Controls 261120-063253-b0630d, 261120-063256-4ba928 and 261120-063300-e7f9cb all **failed to build**
  in 0.5 s ("generator: Ninja does not match the generator used previously: Unix Makefiles", in the
  slot's shared `build/`). None of the three mutants ran, so these jobs are not evidence. My rerun is
  below.
- CI: the remote branch is at 83792cf, not 1cd353f, so neither `asan` nor `tsan` has run on this commit.

## What I checked myself

Scripts are under `.scratch/xapiand/review-consensus-rpc-lane-correctness/` in the Xapiand home
(`review_probe_test.cc` reuses `rpc_host_test.cc`'s helpers. `add_probe.sh` adds it to a checkout).

| Job | What | Result |
|---|---|---|
| 261120-065312-6459c9 (`idle.sh`) | `consensus_transport_idle cost --quick`, 3 runs each, at main 1c08c9f, at 1cd353f, and at 1cd353f with `report()` returning before it touches `reported` when no reporter is set | Hosts 5, loops 1, bytes per idle group, the same in every run: main **283.6**, branch **373.3** (budget 372, fails), patched **283.6** (passes). The `reported` table accounts for all of the +89.7 B (+32%). |
| 261120-065311-8717f2 (`controls.sh`) | The three controls, each with its own build dir, plus the base and the probes in RelWithDebInfo | Base passes `group-full` (25 calls, largest latency 1 ms), `stream` (104 ms largest gap) and `leader`. `rpc-shared-credit` fails as intended: 1 call, largest latency 3,993 ms. `hello-limits-ignored` fails as intended: 1,048,576 B reported, the 512 KiB entry applied on host 3, 1 append carried it. `rpc-one-frame` ends with **SIGSEGV (exit 139)**: a crash, not a failed assertion (F3). Probes: loss, pending and contend pass. |
| 261120-065308-e4b920 (`tsan.sh`) | TSan build: the 5 `rpc_host` cases, the 3 `hello` cases, the probes (`loss`, `pending`, `contend` ×3) | All pass. 0 ThreadSanitizer reports. `contend`: 64 streamed echoes to two peers on host 1's loops 3 and 0, through a 256 KiB outgoing pool and a 512 KiB calling pool, both at high water: 56 ok with 69,950,093 B exact, 8 cancelled, 0 ended twice, every pool on all three hosts back to 0. `loss`: when the peer stops, a 256 MiB stream and a hung call each end once as `connection_aborted`, and the caller's pools return to 0. A call made from inside a `done` ends once. A call to the stopped peer ends `timed_out` after 501 ms with a 500 ms deadline. `pending`: a call waiting on a dial whose handshake never completes ends `not_connected` once at `stop()`. |
| 261120-070936-5f91c4 (`asan.sh`) | ASan+UBSan Debug: the probe's `small-frame` case at the base, and the `rpc-one-frame` mutant's `stream` case | `small-frame`: the constructor accepts the configuration, and the call ends `connection_aborted` with 0 bytes (F3). One-frame: a null `front()` on an empty `chunks_` deque (F12). |

Reading (`git diff origin/main...1cd353f`; host_transport.h, rpc.h, node_hello.h, links.h, connection.h):

1. **Credit and flow.** Lane frames never touch `stats.uncredited` or a group's `outstanding`
   (host_transport.h:749-754). Heartbeats ride Control, which is a separate connection. The pools
   never hold-and-wait:
   - `serving` uses `try_acquire` and answers `overloaded`.
   - `calling` waits in FIFO order while holding nothing.
   - `outgoing` waits while a call holds `calling` bytes. `outgoing` is returned when the connection
     takes the frame (`put_rpc`), never when the peer acts.

   Cancel, timeout and loss return every byte. `Lane::finish` releases `in.held`, cancels a ticket,
   and erases the queued chunks. A grant already in flight is released in `granted()`. The `calls`,
   `loss` and `contend` results above show this with values. A host-wide `outgoing` FIFO lets a
   stalled peer delay other peers' chunks until `dead_after` (5 s) aborts its connection. That is
   bounded.
2. **R14 (a group window below the rpc_bytes frame is not refused) is safe.** `data_ready` sends a
   frame alone when nothing is outstanding (host_transport.h:598). The receiver never checks the
   sender's window: `arrived()` only applies credit. The window is a local sender option. So a frame
   larger than the window travels alone and is credited back, and nothing wedges. The lane's own
   frames are a separate question (F3).
3. **Wire.**
   - The hello is read at exactly 152 bytes (connection.h:104).
   - `decode_node_hello` refuses the wrong size or magic, node 0, a bad lane or flag, zero
     identities, empty or inverted RPC and wire ranges, an RPC version above 9999, a limit past
     `size_t`, and any limits that fail `validate_limits`.
   - Disjoint wire ranges throw `WireVersionRefused`, counted in `LinkStats::version_refused`
     (`version` case: 0 links, 0 frames).
   - Lane input goes through the landed `rpc::decode` and the D3 rules. The receive side bounds it:
     rule 7.6 (past credit), the serving bound, and `maximum_frame` on read (connection.h:273). A
     `DTRP` frame on Control, or on Bulk with the lane off, aborts the connection.
4. **Lifetimes.**
   - Each Lane lives on its connection's loop. `close_rpc` moves it out of the table before `close()`.
     `close()` finishes every call synchronously, and `ending` erases each `RpcCall`, so a stale
     `RpcCall` never resolves to the next Lane's handles. Those handles restart at 1.
   - Cross-loop pool wakes post through `alive_` (a `weak_ptr`) and release when the Lane is gone.
   - Links runs `on_ended` before its task ends (links.h:362-365), so no Lane outlives `drained()`.
   - A `call()` re-entered from a `done` during `close()` finds `peer.lane` null and waits. The
     `loss` probe shows it ends once.
5. **Tests check values.** Bytes are checked one by one against a generator. The tests also check
   codes, cancel text, pool levels, latencies, heartbeat gaps, and the reported limits by field.
   None of the assertions only counts.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The suite is red. `consensus_transport_idle` grows from 283.6 to 373.3 B per idle group at hosts 5, loops 1 (budget 372). Main passes. The cause is the new per-connection `Opened::reported` GroupTable (host_transport.h:486). `report()` inserts into it for every admitted group on every connection (host_transport.h:998), even with no reporter set. A production host sets a reporter, so it would pay the full cost anyway. | Suite 261120-063245-5d4c86. Idle 261120-065312-6459c9 (main 283.6, branch 373.3, no-table 283.6, each 3/3 identical) | Keep one table: store the last reported source as the value of the existing `Opened::groups` entry (`GroupTable<NodeId>` instead of `GroupTable<Empty>`). An admitted frame always has its group open there, so this adds 8 B per entry and no second index. The existing idle test is the test: it must pass in `build-test`. |
| F2 | yes | gap | A1, A3 and A4 have no recorded `--fails-without` job. All three failed at cmake configure (Ninja over the slot's Makefiles `build/`) without running a mutant. | 261120-063253-b0630d, 261120-063256-4ba928, 261120-063300-e7f9cb (build exit 1, 0.5 s) | Resubmit each with its own build directory (for example `-B build-mut`) or `--fresh`. My rerun (261120-065311-8717f2) predicts that shared-credit and limits-ignored fail for the right reason. One-frame crashes instead (F12). |
| F3 | no | defect | The constructor accepts a `maximum_frame` below the lane's own frame size. R14's check covers the rpc_bytes frame (host_transport.h:282) but not a 64 KiB `Body` chunk plus its header. Probe: 16 KiB rpc_bytes passes validation with a 32 KiB `maximum_frame` (rpc_bytes frame 16,997 B). A streamed call then ends `connection_aborted` with 0 of 262,144 B, and the Bulk connection carrying group frames aborts with it. Here `Connection::send` threw `frame size` (connection.h:126) inside `read()`'s feed and the reader caught it. On the posted `rpc_ready` feed path, the same throw would leave an asio handler (not shown). A peer whose bound is below 64 KiB fails the same way on read. | 261120-070936-5f91c4 (`small-frame`, ASan, exit 1). host_transport.h:282 | This narrows accepted configurations, so it is the owner's call. Options: (a) refuse in the constructor a `maximum_frame` below the lane's largest frame, beside the rpc_bytes check, tested in `hello_test version` (recommended: it is R14's shape); (b) chunk at min(chunk_bytes, the frame bound less the header) and carry `maximum_frame` in the hello, a wire change; (c) keep it and document the floor. |
| F4 | no | gap | T7 (late-fire rule for call deadlines) is not built. Correctness holds without it: a late expiry only ends a call `timed_out` or `deadline_exceeded`, which are defined outcomes. But the design's soak (rpc-lane.md A5) uses `late-deadline` as its own `--fails-without`. This quest's When done runs that soak: 1,000 streamed calls with zero errors, under the load that broke `consensus-fetch-macos`. Expect it to fail without T7. | docs/design/rpc-lane.md:282, 293 | The lead's call: assign T7 (`late_deadline`'s one rule, deadline.h) before the soak, or accept timeouts in it. |
| F5 | no | gap | Deferring `lane_idle`/`Drain` is safe for correctness. `stop()` aborts, so calls in flight end `connection_aborted` (outcome unknown) instead of `not_processed` (safe to retry). Callers must already handle that after a crash. The cost is resources: a Bulk connection (~47-51 KB per end, idle bench) stays open to every node ever called until `stop()`, which matters at the 100k-node goal. `hold(node)` is moot while nothing closes, but it must land together with `lane_idle`. | rpc-lane.md (mechanism) "Not built here". Idle bench "per connection end" | Track both in `cluster`, which consumes the lane. Not needed for this quest's criteria. |
| F6 | no | nit | DTHL0002 keeps its magic but grows from 64 to 152 B. A stale 64-byte binary would decode the first 64 B and read the rest as frames. The wire range versions frames, not the hello's own layout. Formats stay open until release (R13), so this is safe today. | node_hello.h:31, connection.h:104 | Options: keep it (R13), or rename to `DTHL0003`, which costs nothing and fails a stale peer at the magic. |
| F7 | no | defect | A `Serve` handler that calls `total()` inside the handler, before `Lane::called` inserts the call, throws `std::out_of_range` (rpc.h:414) from inside `receive()`. `rpc_arrived`'s `catch (...)` then charges that to the peer as malformed and aborts the Bulk connection, groups included. `write()` there is silently dropped and `stop()` does nothing. More generally, any exception from a local callback is blamed on the peer. | rpc.h:688, 414. host_transport.h:1139 | Document that a handler acts only from its events. Alternatively, catch only `ProtocolError` and `journal::Corruption` as the peer's fault and let local bugs propagate. |
| F8 | no | nit | Calls to a peer whose hello is refused by version end `timed_out` at their deadline (R11 redials), not with a named outcome. The refusal itself is named and counted. | host_transport.h:723. links.h `version_refused` | Optional: end waiting calls `protocol_not_supported` when a dial fails with `WireVersionRefused`. |
| F9 | no | nit | `Call` frames with up to 64 KiB of inline request go out as "small" frames, ahead of group control and data on Bulk with no alternation. Bounded by `rpc_outgoing_bytes` (64 MiB). Heartbeats ride Control and are unaffected. | host_transport.h:749. rpc.h:651 | Optional: alternate inline Calls like chunks. |
| F10 | no | nit | The pools are declared after `loops_` (host_transport.h:1193-1194), so they are destroyed before any Lane still in a table. This is safe under the documented "destroy after drained" contract (every Lane is closed by then), but fragile. | host_transport.h:254, 1193 | Declare the pools before `loops_`. |
| F11 | no | nit | Before a peer's first frame, a leader caps its appends by its own limits only (Core M8), so an entry above the peer's limit can go once. Only the host application wires `on_peer_limits` to `Driver::peer_limits`: no library code does, and `leader` feeds Core by hand. | core.h:381-397. hello_test.cc `leader` | `cluster` must wire it. Note for its A5. |
| F12 | no | gap | The `rpc-one-frame` control fails by crashing, not through `stream`'s checks. Its 32 MiB `Body` makes `encode` throw inside `chunks_[c.id].push_back(...)` (rpc.h:654). That leaves an empty deque in `chunks_`, and the next `feed` reads `front()` of it (rpc.h:488 from host_transport.h:922). ASan reports a null read, and the process exits with SIGSEGV. `--fails-without` counts a crash as a pass, so this control would pass even if `stream` checked nothing. | 261120-065311-8717f2 (exit 139). 261120-070936-5f91c4 (ASan stack) | A mutant that stays within `chunk_bytes` but ignores credit (`sendable` returns `chunk_bytes`). The receiver's rule 7.6 then aborts the connection, and `stream` fails on its outcome check. The lead decides whether A3 needs it. |
| F13 | no | nit | `Lane::chunk` is not exception-safe: `chunks_[c.id]` is created before `encode` runs (rpc.h:654). The code is in rpc.h, landed before this quest. Today it is reachable only past `send()`'s own bound, as in F12. | rpc.h:654 | Encode first, then push. |

## Not run

- CI (`asan`, `tsan`) at 1cd353f: not pushed. My local TSan job covers the lane and hello cases only,
  not the rest of the suite.
- The When-done jobs: the fuzz hours, the 1,000-call macOS soak and the 4 GiB strict run. The fuzz
  corpus is being regenerated elsewhere.
- Linux. All my jobs ran on this macOS host.
- A Driver-integrated PeerLimits path: no host in the library wires it (F11).
