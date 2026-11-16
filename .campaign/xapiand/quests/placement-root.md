# placement-root: The root authority as a strata group: nodes and namespace records

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status placement-root`.

## Outcome

One strict-consensus group, `strata::placement`, with 3 or 5 voters (a lone voter on one node),
holds the cluster's authority as a strata index keyed by path (design D5): a node directory (id,
name, addresses, certificate pin, attributes, state) and one namespace record per index (path,
shard count, `rf`, settings version, and per shard its opaque group id, incarnation and members).
Its checkpoint is a strata manifest, its memory strata's bounded caches. Placement chooses `rf`
distinct nodes with the fewest groups. Records are scanned in key order by a caller-supplied
matcher that can seek past keys it cannot match, so Xapiand's `Glob` drives a pattern scan under a
work bound (F4).

## Acceptance criteria

- A1 Create, read and delete of nodes and namespace records, with nested paths (`twitter`,
  `twitter/user`) distinct records (tests with values)
- A2 Placement puts `rf` distinct nodes per shard and spreads leaders within one per node
  (a property test)
- A3 The authority survives the loss of any minority of its voters with no committed record lost
  (a simulation-style test)
- A4 The seekable scan returns exactly the keys a brute-force filter returns, for a reference glob
  matcher with `*`, `?`, classes, `**`, `_all`, a leading `*`, `**/x`, `-` exclusions and `.` names
  (never listed), nested paths included; and the keys it visits for a pattern that matches one
  subtree stay within a stated multiple of that subtree's size, not the whole set (a property test
  with values, and a counter)
- A5 At 10^6 namespace records of 5 shards each, records only: resident memory at most 256 MiB;
  checkpoint time and bytes; record creations per second from 64 concurrent clients, at least
  1,000, with p50 and p99; a node's cache size at its default (10^4 entries); each stated against
  D5's estimates (an `exclusive` job)
- A6 [artifact] `docs/mechanisms/placement-root.md`, with the record layout and the scan's bound

## Contracts

Provides `strata:placement-records` (bytes): the node and namespace records, their key order, the
seekable scan API, and the root's checkpoint. Consumes `strata:group`. `format-freeze` reviews the
records.

## Decisions

- Design D5, D6, F6, F17: the group's log is authoritative for membership; the record follows and is
  reconciled after a crash; no leader hints are written to the root; no new group or incarnation
  while an old member may live.
- Lead (F4): `_all` and `**/x` match at any depth, so a pattern scan is not always a prefix scan;
  the matcher seeks, and Xapiand bounds the scan with `walk_gate` (distributed-search A4).
- Lead: A5 measures records only; the end-to-end creation rate, group creates included, is
  shard-groups A8.

## May land early

The node directory with A1-A3 behind tests, before the namespace records' layout is reviewed.
