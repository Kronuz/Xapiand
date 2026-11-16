# shard-groups: Shards across nodes: routing, members, replicas, linearizable reads

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status shard-groups`.

## Outcome

An index's shards live as groups on `rf` nodes chosen by the root authority, and any node serves
any request (design D5, D7, D8). Creating an index on first write commits its namespace record at
the root, then creates `rf` × shards groups on the chosen nodes. A write goes to the group leader
with at most one forward and one redirect followed by the receiving node; a second miss or a group
without quorum answers 503 with `Retry-After`. `read=linearizable` reads at the leader after its
quorum probe; `read=prefix` reads any replica's last refresh. A node maps an index path to its own
local groups without the root, at a stated memory cost (F6). This quest also wires the cluster
options (`--join`, `--replace-after`, the certificate options) and the manager (F25), and serves
`GET /:nodes` and `GET /`'s `cluster_name` and `nodes`.

## Acceptance criteria

- A1 Section 1's checklist items 1, 2, 5 and 6 pass on 3 and 5 processes (the `cluster` check)
- A2 A write to any node takes one forward with a fresh hint and at most two with a stale one,
  counted per request in a job
- A3 A 503 under no quorum and on a second miss, and the receiving node follows the redirect, never
  the client (tests with statuses and hop counts)
- A4 A cancelled proposal answers an unknown outcome, and its retry is applied once (a test)
- A5 Through the proxy capped at 100 Mb/s, a 16 MiB buffered document with a stored blob and a
  256 MiB streamed upload commit at `rf = 3` while another group on the same node pair keeps its
  heartbeat gaps under the 1 s minimum election timeout; on loopback a 4 GiB upload does the same,
  with its disk use (about 24 GiB) stated; a control with `frame_bytes` 0 and `command_bytes`
  64 MiB fails the capped case (`--fails-without`, jobs)
- A6 A 4 GiB upload in flight while leader balancing runs on its node completes without abort (F26;
  a job, with placement-repair's A4 mutant failing it)
- A7 After a replace, `rf` 1 to 3, and a drain, every stored blob reads back byte-equal from the new
  replica; a mutant install without the file set fails (a job, `--fails-without`)
- A8 Index creation end to end (F6): creations per second from 64 concurrent clients on 3 processes,
  root commit and `rf` × shards group creates included, at 1 and 5 shards and `rf` 1 and 3, with
  p50 and p99 first-write latency against master's create on first write (an `exclusive` job;
  D5's estimate, at least 1,000 per second per cluster, is the stated bar, a miss reported)
- A9 Path to local groups (F6): after a restart with the root unreachable, a node serves its own
  groups for every index it holds, nested paths included; the memory of that map stated at 10^4
  and 10^6 local groups (a test, and a job)
- A10 With the old leader partitioned by the proxy, its stale get-by-id at `prefix` is flagged by
  the checker, and at `linearizable` the history passes (a job)
- A11 `GET /` and `GET /:nodes` match the authority's directory on 3 processes (a harness job)
- A12 [decision] cluster-release Q8, the routes, pending owner; lead default (a), the `:` routes
  and the per-index checkpoint on `COMMIT /idx/`
- A13 [artifact] `docs/mechanisms/shard-groups.md`, with the mapping from a path to local groups
  and its cost

## Contracts

Provides `xapiand:shard-map`: the node's shard-map cache (its own groups from local group metadata,
plus an LRU of root records filled on a miss and invalidated by a redirect, an unknown-group answer
or an incarnation mismatch, at `RESOLVER_CACHE_SIZE`), the forward and redirect rules, and the
routes above. Consumes `strata:group-install`, `strata:placement-records`, `xapiand:shard-rpc`,
`xapiand:node-directory` and `xapiand:command`.

## Decisions

- Design D5, D7, D8, D12, D15 (the 1,024-bucket id hash stays; the shard count is fixed at create);
  F17, F23.
- Lead (F6): the design leaves open how a node maps a path to its own groups at 10^6 indexes per
  host. The contributor proposes the mapping (for example a local strata index of path to group id
  written at group create, recovered with the groups) in the A13 note before building it; the lead
  rules on it, and A9 measures it.
- Lead (F25): the options and manager wiring moved here from cluster-membership.
- Lead: lands after `detent:consensus-fetch-macos`, since replicas install over the fetch path.

## May land early

None.
