# shard-rpc: The node-to-node shard service

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status shard-rpc`.

## Outcome

Xapiand nodes call each other over consensus's request lane (design D9): `forward_write`, `stats`,
`search_partial`, `get`, `count`, `info` and `keepalive`, with msgpack bodies versioned through the
hello's range. `get` of a stored blob up to 4 GiB and `forward_write` of a streamed upload stream
their bodies in chunks under the lane's credit. Errors map to Xapiand's HTTP statuses at the
receiving node. This quest builds the service and its client; shard-groups and distributed-search
call it.

## Acceptance criteria

- A1 Each method round-trips between two in-process nodes with values asserted, including the error
  mapping (every error class to its status), deadlines and cancellation (tests, run as a job)
- A2 A 4 GiB `get` and a 4 GiB `forward_write` stream end to end with the bytes' SHA-256 equal and
  resident memory under a stated bound, never the body's size (a job)
- A3 An N − 1 peer, or a peer outside the hello's version range, is refused with a named error
  (a test)
- A4 One CPU-hour of fuzzing per msgpack decoder, zero findings (the `fuzz` milestone check)
- A5 [artifact] `docs/mechanisms/shard-rpc.md`, with the methods and their bodies

## Contracts

Provides `xapiand:shard-rpc` (wire): the methods and their msgpack bodies, versioned by the
hello's range. Consumes `consensus:rpc-lane`. `format-freeze` reviews these bytes.

## Decisions

- Design D9; D8 (a cancelled or timed-out `forward_write` reports an unknown outcome; retries are
  idempotent by version).
- Lead: sources go in `src/cluster/` and tests in `tests/cluster/`, both globbed by the prerequisite
  quick fix, so this quest needs no `CMakeLists.txt` and runs beside journal-replaces-wal.

## May land early

None: it is a wire format.
