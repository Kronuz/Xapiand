# consensus-rpc-lane: A request lane on the host transport, streamed bodies

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-rpc-lane`.

## Outcome

consensus's host transport (`detent:consensus-transport`, one connection per node pair, D16
multiplexing) carries a request/response lane for application calls beside the group frames
(design D9, section 8 items 1, 4 and 5). It generalizes the `DTFT0001` fetch lane: a call names a
method id, carries a deadline and can be cancelled; bodies are opaque bytes, and a body may stream
in chunks under the lane's own byte credit, so a 4 GiB blob never travels as one frame. The lane
opens connections between any two nodes, not only those sharing a group. Detent's history fetch
(Detent msg-44, option 1) is one method on it.

## Acceptance criteria

- A1 Calls with deadlines and cancellation over shared connections; a full group credit does not
  delay an RPC, and a full RPC credit does not delay a heartbeat (the `reader_test` shape, with a
  control that shares one credit and fails): tests with values, run as a job
- A2 A 4 GiB streamed body crosses under the lane's credit while a heartbeat between the same two nodes (on Control; the body rides Bulk, design D1)
  keeps every gap under the 1 s minimum election timeout (async-drivers D10); the job states the
  largest gap and the throughput, and a control sending the body as one frame fails
  (`--fails-without`)
- A3 A call between two nodes that share no group connects, completes and is accounted to no group
  (a test)
- A4 Xapiand's methods (at least one real call, e.g. a remote search) run over the lane that
  `detent:consensus-transport` provides, with streamed bodies; the lane's base frames, fetch as
  method 1 and DTFT0001's removal are that quest's, not this one's (a test)
- A5 Under the load that broke `detent:consensus-fetch-macos` on macOS, a streamed body's bytes
  equal the source's SHA-256 over 1,000 calls with zero errors (a job on macOS, and on Linux in CI)
- A6 One CPU-hour of fuzzing on the lane's frame decoder, zero findings (the `fuzz` milestone check)
- A7 [artifact] `docs/mechanisms/rpc-lane.md`

## Contracts

Consumes `consensus:rpc-lane` (wire) from `detent:consensus-transport`, whose A7 builds the lane's
frames, call table, versioning through the `DTHL0002` hello and lane-only connections; this repo's
`docs/design/rpc-lane.md` stays the one spec of those bytes, with every decision marked by who builds
it. This quest provides streamed request bodies and Xapiand's methods on top and changes no byte:
the request-direction fields (`Call.total`, request `Body` and `Credit`) are in version 1.
`format-freeze` reviews these bytes.

## Decisions

- Lead (2026-11-16, design review F4): a call racing an idle close or shutdown is settled by a
  `Drain` frame carrying the last accepted call id (HTTP/2 GOAWAY's shape), sent before any close;
  Detent's transport quest builds it with the other frames.

- Scope (Detent's lead, 2026-11-16): `detent:consensus-transport` owns the request/response lane
  and moves fetch onto it (criterion A7 holds T1-T7). This quest adds streamed request bodies and
  Xapiand's methods on top, and never ports today's `Peers` fetch (D102: no building on code about
  to be deleted).

- Design D9: one transport between nodes; a second TLS listener or HTTP between nodes is
  rejected.
- Lead: this quest is in Detent's library; tell Detent's lead when it starts
  (`campaign message detent:consensus-transport ...`), since its history fetch moves onto the lane.

## May land early

None: it is a wire format.
