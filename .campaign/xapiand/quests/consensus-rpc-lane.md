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
- A4 Detent's history fetch is expressed as one method, with its existing test passing over the
  lane (a test)
- A5 Under the load that broke `detent:consensus-fetch-macos` on macOS, a streamed body's bytes
  equal the source's SHA-256 over 1,000 calls with zero errors (a job on macOS, and on Linux in CI)
- A6 One CPU-hour of fuzzing on the lane's frame decoder, zero findings (the `fuzz` milestone check)
- A7 [artifact] `docs/mechanisms/rpc-lane.md`

## Contracts

Provides `consensus:rpc-lane` (wire): the lane's frames (method id, call id, deadline, cancel,
credit grant, body chunk, end, error), versioned through the `DTHL0001` hello's range. Consumes
`consensus:transport` from Detent's quest. `format-freeze` reviews these bytes.

## Decisions

- Design D9: one transport between nodes; a second TLS listener or HTTP between nodes is
  rejected.
- Lead: this quest is in Detent's library; tell Detent's lead when it starts
  (`campaign message detent:consensus-transport ...`), since its history fetch moves onto the lane.

## May land early

None: it is a wire format.
