# The shard RPC: Xapiand's calls between nodes

`src/cluster/rpc.{h,cc}` carries Xapiand's node-to-node calls on consensus's request lane
(consensus `docs/mechanisms/rpc-lane.md`), on the Bulk connection the host transport keeps per node
pair (cluster-release D9). There is no second listener and no HTTP between nodes: the node
certificate that authenticates the transport authenticates every call, and `ServerCall::node()` is
the caller as its certificate names it.

## Methods

| Id | Method | Used by |
|---|---|---|
| `0x58530001` | `forward_write` | a write relayed to its group's leader, the body streamed (D8) |
| `0x58530002` | `stats` | a search's statistics round (D10) |
| `0x58530003` | `search_partial` | a node's shards' partial result (D10) |
| `0x58530004` | `get` | a get-by-id at the leader, a stored blob streamed (D4) |
| `0x58530005` | `count` | a count's per-node part |
| `0x58530006` | `info` | `:info` and health |
| `0x58530007` | `keepalive` | a node's 500 ms keepalive to the root leader (D6) |

The ids start at `0x58530000` ("XS") so other users of the same transport keep their own.

## A call's bytes

Each direction of a call is one lane stream:

```
request:  u32 version | u32 envelope length | envelope | payload
response:                u32 envelope length | envelope | payload
```

Integers are little-endian. The envelope is the method's message, buffered whole, at most
`envelope_bytes` (16 MiB, the largest buffered body Xapiand accepts, `search_service.h`); a
document body or a stored blob is the payload, streamed in chunks under the lane's credit, so a
4 GiB upload or blob never sits in memory. A stream's total is known (`8 + envelope + payload`)
or `unknown_total`, ended by the producer. A producer pushes while `queued()` is under a chunk or
two and again on `writable`; the receiver consumes as it takes bytes, which renews credit.

The server runs the method's handler once the envelope is in. Payload bytes that arrive before
the handler sets its sink wait unconsumed, so they hold the caller's credit instead of memory
beyond the window.

## Versions

Every request carries the caller's version of this service. A server outside its range
(`Versions`, `[1, 1]` in this build) answers `version_refused` with its range as text,
`"min max"`, before it runs any handler; the caller sees `Failure::peer`. A version word with its
high half set or zero is `bad_request`. The lane's own version, agreed by the hellos
(`Link::rpc`), only chooses the lane's frame magic: it is consensus's, not this service's.

## Failures

A server's failure is the lane's `Error` frame with code `256 + Status` and a text of at most
1,024 bytes. The caller gets a `Failure`: the server's `Status`, or the lane's outcome as a
`Fault`. `retry_safe()` says the server did not apply the call, so any call may go again;
`http_status()` is what a receiving node answers when it gives up.

| Failure | Meaning | Retry-safe | HTTP |
|---|---|---|---|
| `version_refused` | outside the server's range; text `"min max"` | yes | 502 |
| `not_leader` | text: the leader hint's node id, or empty | yes | 503 |
| `unavailable` | refused before proposing: no quorum | yes | 503 |
| `unknown_group` | no such group or incarnation here | yes | 503 |
| `not_found`, `conflict`, `bad_request`, `too_large` | the method's answer | yes | 404, 409, 400, 413 |
| `unknown_outcome` | proposed, then cancelled or timed out | no | 504 |
| `internal` | | no | 500 |
| `expired` | the deadline passed before the call was sent | yes | 503 |
| `timed_out` | the deadline passed after it was sent | no | 504 |
| `cancelled` | this end stopped it | no | 504 |
| `not_connected`, `not_processed`, `overloaded` | never reached a handler | yes | 503 |
| `aborted` | the connection ended with the call live | no | 504 |
| `unknown_method`, `protocol` | the peer serves no such method or framing | yes | 502 |
| `message_size` | an answer envelope past `envelope_bytes` | no | 502 |
| `handler_failed` | the peer's handler threw; text: its message | no | 500 |

A deadline set by the caller travels in the `Call` frame; the lane ends both ends at it, the
caller with `Cancel deadline_exceeded` and the server with `Error deadline_exceeded`, and the
server's `done` tells its handler, which stops its work. A cancel does the same with `cancelled`.

## Threads

Everything runs on the call's loop, `HostTransport::loop_of_node(node)`: `Client::call`, the
handlers, `ServerCall`'s methods and the callbacks. `Client::unary` and `ServerCall::post` may be
called from any thread and post there; a handler that works on another thread posts its reply
back with `post`, which drops it once the call ended.

## Evidence

`tests/cluster/test_rpc.cc`, two hosts over loopback TLS with certificates made in process, one
ctest entry per case: `unary` (a 100,000-byte envelope reversed and a 1 MiB payload echoed),
`streams` (32 MiB each way under credit, every byte checked, the pools back to 0), `statuses`
(each Status with its text, HTTP status and retry-safety; `not_leader`'s hint), `handler-throws`,
`versions` (a newer caller and an N - 1 caller refused by name, no handler run), `deadline`
(both ends near 300 ms), `cancel` (each end once), `lane-faults` (`unknown_method`,
`not_connected`) and `framing` (an envelope past the bound refused before any is buffered, a bad
version word). Until the top-level build pins consensus, `tests/cluster/CMakeLists.txt` builds
them standalone.
