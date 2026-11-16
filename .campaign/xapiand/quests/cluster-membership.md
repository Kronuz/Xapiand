# cluster-membership: Node identity, --join, certificates, keepalives, /:nodes

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cluster-membership`.

## Outcome

A Xapiand node has an identity in the root authority's node directory (design D6): it finds the
cluster from `--join` seeds (host names resolved by consensus's DNS dialing), presents its node
certificate (D97 Q7, one per node), and is refused when it belongs to another cluster or its pin is
not in the directory. Liveness is the root leader's volatile keepalive table: each node keeps one
request-lane connection to the root leader and calls `keepalive` every 500 ms; 3 misses mark it
`suspect` then `down`, and only transitions are logged. `/:nodes` and `GET /`'s node list read the
directory. This quest owns only `src/cluster/{node,membership}*` and its tests (F25): the options
and the manager wiring are shard-groups'.

## Acceptance criteria

- A1 Three nodes form from one seed; a node from another cluster, and a node whose certificate is
  not pinned, are refused with named errors (tests, and a harness job on the process tier)
- A2 The node list the membership module serves matches the authority's directory after joins,
  a leave and a `down` (a test with values; the HTTP route is shard-groups' wiring)
- A3 Plaintext peers only behind an explicit flag and only on loopback; the flag on a non-loopback
  address is refused at startup (a test)
- A4 A node that shares no group with the root leader keeps its keepalives, and its `down` is logged
  after 3 misses of 500 ms, within 2 s of the kill (a harness job with the times stated)
- A5 Keepalives per second at the root leader as a function of the node count, measured at 3, 5
  and a simulated 300 (R10's row; a job)
- A6 [decision] cluster-release Q5 and Q7, pending owner; lead defaults (b) Kronuz/pki's offline CA
  with `--peer-insecure` on loopback only, and (a) multicast discovery removed
- A7 [artifact] `docs/mechanisms/cluster-membership.md`

## Contracts

Provides `xapiand:node-directory`: the module's API over the root's node records (join, leave,
liveness events, the node list). Consumes `strata:placement-records` and `xapiand:shard-rpc`'s
`keepalive` method.

## Decisions

- Design D6, F22; F25 (recommended option: this quest owns only `src/cluster/{node,membership}*`
  and its tests; shard-groups does `src/opts.*` and `src/manager.*`; the plan's edge on
  journal-replaces-wal is dropped).
- Lead: `needs shard-rpc` is added to the plan's edges, because `keepalive` is shard-rpc's method
  (design section 5's contract 11) and A4 runs on it.
- Lead: A1 and A4's harness jobs start the module from a test binary under `harness/cluster/`,
  since the `--join` option is wired by shard-groups.

## May land early

None: it is a security surface.
