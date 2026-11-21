# placement: The root authority places and repairs groups

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status placement`.

Whole feature (replan-2026-11-20, section 1 F2). Folds `placement-root` and `placement-repair`.

## Outcome

One strict-consensus root group holds the node directory and one namespace record per index, keyed by
path, with nested paths distinct. Placement chooses `rf` distinct nodes with the fewest groups and
spreads leaders. Records are scanned in key order by a caller's matcher that seeks past keys it
cannot match, so Xapiand's `Glob` drives a pattern scan under a work bound. The root keeps every
shard at its `rf`: a node down past the cluster setting `replace_after` (default 300 s, held in the root, changed at runtime; not a node flag) is replaced, a drain moves a node's replicas,
`number_of_replicas` grows and shrinks through D7's steps, leader balance defers while a chunked
command is open, and a new root voter installs the root's records.

## Acceptance criteria

Gate: the suite (`build-test`) and CI (`asan`, `tsan`). Every criterion is a test the suite runs.

- A1 Nodes and namespace records create, read and delete, with `twitter` and `twitter/user`
  distinct; placement puts `rf` distinct nodes per shard and at most one leader more than the mean
  per node (a property test); the authority survives the loss of any minority of voters with no
  committed record lost (a simulation-style test)
- A2 The seekable scan returns exactly what a brute-force filter returns for the reference glob
  matcher (`*`, `?`, classes, `**`, `_all`, a leading `*`, `**/x`, `-` exclusions, `.` names never
  listed, nested paths), and the keys it visits for a one-subtree pattern stay within a stated
  multiple of that subtree: a property test with a counter
- A3 Replace past `replace_after` (a setting changed at runtime takes effect without a restart), drain, and `number_of_replicas` 0, 1, 2, 1, 0 under writes lose
  no acknowledged write, leave digests equal after each step, and keep every shard at `rf` wherever
  a replacement exists; an `rf = 1` group waits for its node: simulation-style tests
- A4 Leader balance defers a transfer while the group reports an open multi-entry command and makes
  it once the command ends; a mutant that transfers anyway aborts the command: a test with values,
  the mutant a `--fails-without` job
- A5 [artifact] `docs/mechanisms/placement-root.md` and `docs/mechanisms/placement-repair.md`

## Tasks

One agent each, merged into the quest branch on its own tests. Strong, high (the records are
pre-release bytes, D30).

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `root` | `include/strata/placement/**` and `src/placement/**` (root, records, placement), `test/placement/root*`, `bench/placement/**`, `docs/mechanisms/placement-root.md` | implementation, strong, high | Node directory, namespace records, placement, leader spread (A1) |
| 1 | `scan` | `src/placement/scan*`, `include/strata/placement/scan*`, `test/placement/scan*` | implementation, strong, high | The seekable matcher scan and its work bound (A2), on `root`'s record keys |
| 2 | `repair` | `src/placement/repair*`, `test/placement/repair*`, `docs/mechanisms/placement-repair.md` | implementation, strong, high | Replace, drain, `number_of_replicas` steps, leader-balance deferral, new root voter install (A3, A4), against `group-heal`'s contract (its quest file) |

Order 2 starts once `root`'s record API is merged. `group-heal` lands first; `repair` builds against
its contract and runs beside it, integrating on its `quest/xapiand/group-heal` branch with `--ref`.

## Contracts

Provides `strata:placement-records` (node directory, namespace records, the seekable scan) and
`strata:placement-repair`. Consumes `strata:group` and `strata:group-install`. The records are
pre-release bytes (D30); `format-freeze` reviews them.

## Decisions

- Needs `group-core`; `after group-heal` (repair uses its learner and membership steps).
- Nested index paths are core (`twitter` and `twitter/user` distinct).
- Namespace keys sort in path order (lead, 11-20): a key stores `/` as byte 0 (names hold no
  NUL), so `twitter` < `twitter/user` < `twitter-a`; a path's subtree is the contiguous range
  [`P` p `\0`, `P` p `\1`) right after it, and a scan reads each record once. The preferred
  leader is the node with the fewest leaderships (root's ruling, accepted).
- Each task registers its tests in `test/placement/<task>.cmake`; `CMakeLists.txt` is the lead's
  (quest branch 4a42c30c).

## When done

Runs once, when the feature is complete and before it lands; findings are fixed on the quest branch,
each with a test that fails without it.

- Review: one reviewer, lens `correctness` (the records are pre-release bytes, D30).
- Measurements, one `exclusive` job of at most 10 minutes each on the macOS host (D59): the root at
  10^6 namespace records of 5 shards (memory at most 256 MiB, checkpoint time and bytes, creations
  per second from 64 clients with p50 and p99, a node's cache at 10^4 entries), and a new root
  voter's install of those records against D5's 2.5 s per 300 MB. The cluster gate's R10 cites them.

## May land early

None: the records are bytes (D30) and the scan and repair are what the review exists for.
