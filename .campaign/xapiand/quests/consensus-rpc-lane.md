# consensus-rpc-lane: The cluster transport: request lane, streamed bodies, hello limits, version range

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-rpc-lane`.

Whole feature (replan-2026-11-20, section 1 F3). Folds `consensus-hello-limits` into this in-flight
quest: both change `include/consensus/transport/**` and the host transport, and both are what
`cluster`'s shard RPC needs from consensus (the lane, and the version range its A5 refuses on).

## Outcome

consensus's host transport (`detent:consensus-transport`, one connection per node pair, D16
multiplexing) carries a request/response lane for application calls beside the group frames (design
D9, section 8 items 1, 4 and 5). It generalizes the `DTFT0001` fetch lane: a call names a method id,
carries a deadline and can be cancelled; bodies are opaque bytes, and a body may stream in chunks
under the lane's own byte credit, so a 4 GiB blob never travels as one frame. The lane opens
connections between any two nodes, not only those sharing a group. The node hello carries the node's
ten `Limits` and a wire version range; the host reports `PeerLimits` per incarnation and refuses a
frame or window below what its `rpc_bytes` needs. Detent's history fetch (Detent msg-44, option 1)
is one method on the lane.

## Acceptance criteria

Gate: the suite (`build-test`) and CI (`asan`, `tsan`).

- A1 Calls with deadlines and cancellation over shared connections; a full group credit does not
  delay a call and a full call credit does not delay a heartbeat (the `reader_test` shape, with a
  control that shares one credit and fails): tests with values, the control a `--fails-without` job
- A2 A call between two nodes that share no group connects, completes and is accounted to no group:
  a test
- A3 A streamed body crosses under the lane's credit, on Bulk, while a heartbeat between the same two
  nodes (on Control, design D1) keeps every gap under the 1 s minimum election timeout (async-drivers
  D10), at a suite-sized body; a control sending the body as one frame fails (`--fails-without`)
- A4 A leader holds back an entry above a peer's hello-reported command limit, and a mutant that
  ignores the reported limits fails that test (`--fails-without`); a hello whose limits fail
  `validate_limits` is refused; a peer outside the version range is refused with a named error: tests
- A5 [artifact] `docs/mechanisms/rpc-lane.md` and `docs/mechanisms/hello.md`

## Tasks

One agent each, merged into the quest branch on its own tests. Strong, high (wire format and a
peer-refusal surface). They share the host transport, so they run in order.

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `lane` | `include/consensus/transport/**`, `test/transport/rpc*`, `fuzz/rpc*`, `fuzz/fuzz_decoders.cc`, `test/transport/peers_test.cc`, `docs/mechanisms/rpc-lane.md`, `docs/design/rpc-lane*`, `CMakeLists.txt`, `CHANGELOG.md` | implementation, strong, high | Calls, deadlines, cancellation, streamed request bodies under the lane's credit, lane-only connections (A1 to A3); the contributor already working on it |
| 2 | `hello` | `include/consensus/host*`, `src/host*`, `src/transport*`, `test/host*`, `test/transport/hello*`, `docs/mechanisms/hello*`, `docs/design/hello*` | implementation, strong, high | The hello carries the ten `Limits` and a version range; `PeerLimits` per incarnation; the leader's hold-back; refusals (A4). After `lane` merged |

## Contracts

Consumes `consensus:rpc-lane` (wire) from `detent:consensus-transport`'s landed parts (the lane's
frames, call table and lane-only connections; `detent:rpc-lane-codec`, `detent:transport-*`); this
repo's `docs/design/rpc-lane.md` stays the one spec of those bytes, with every decision marked by who
builds it. This quest provides streamed request bodies and the hello (`consensus:hello`: the ten
`Limits` and the version range, `DTHL0002`). The request-direction fields (`Call.total`, request
`Body` and `Credit`) are in version 1. `format-freeze` reviews these bytes.

## Decisions

- Lead (2026-11-16, design review F4): a call racing an idle close or shutdown is settled by a
  `Drain` frame carrying the last accepted call id (HTTP/2 GOAWAY's shape), sent before any close;
  the transport's landed parts build it with the other frames.
- Scope (Detent's lead, 2026-11-16): the transport owns the request/response lane and moves fetch
  onto it. This quest adds streamed request bodies and the hello on top, and never ports today's
  `Peers` fetch (D102).
- Re-plan 2026-11-20 (owner: whole features, reviewed and measured once when done): the three
  Xapiand method shapes (old A4) move to `cluster`'s A5, where the real remote search runs; the
  4 GiB body, the 1,000-call macOS load and the fuzz hours move to "When done". The edges on
  `detent:consensus-transport` and `detent:reader-yield-stall` are removed (D123: dropped; their
  parts landed).
- Design D9: one transport between nodes; a second TLS listener or HTTP between nodes is rejected.
- Lead: this quest is in Detent's library; tell Detent's lead when it starts
  (`campaign message detent:consensus-transport ...`).

## When done

Runs once, when the feature is complete and before it lands; findings are fixed on the quest branch,
each with a test that fails without it.

- Review: one reviewer, lens `security`, carrying correctness (a wire format and a peer-refusal
  surface).
- `--later` light jobs: one CPU-hour of fuzzing each for the lane's frame decoder and the hello's
  decoder (the `fuzz` check; zero findings); 1,000 streamed calls on macOS with SHA-256 equal and
  zero errors under the load that broke `detent:consensus-fetch-macos`. One `strict` job of at most
  10 minutes: a 4 GiB body with the largest heartbeat gap and the throughput stated. Track C's C4
  report (`track-c-gates` A2) cites them.

## May land early

None: it is a wire format.
