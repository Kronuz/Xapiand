# cluster-vm-tier: journal

State: working
Quest: .campaign/xapiand/quests/cluster-vm-tier.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

First dev run of `tools/vm/cluster.sh check --nodes 3` (light job 261118-105737-42100a) at
Xapiand:74995cfa6: boots 3 guests, probes multicast, plants and replays.

## Next

1. Fix what the dev run shows; then a parking test (caller SIGKILL, guardian SIGKILL, SIGTERM, down)
   as a light job.
2. A1 as an exclusive job: `campaign job submit --quest cluster-vm-tier --for A1 --level exclusive
   --minutes 20 -- tools/vm/cluster.sh check --nodes 3`.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Guests boot overlays on document-gate's pristine `base.img` (its own `disk.qcow2` also backs on it),
  with a seed that installs nothing: python3, dmsetup, losetup and mkfs.ext4 ship in the image.
- Overlays use `cache=unsafe`: deleted at park; power loss is modeled in the guest by dm-log-writes.
- One disk server per guest owns the disklab, since a lab lives as long as its opener.
- SSH ports skip 18782 and 18784 (document-gate) even when free; the L2's host UDP port is the first
  SSH port + 10000, so concurrent runs never share a wire.
- faultlab pinned at a3b71d1 in harness/cluster/vm/.faultlab, separate from run.sh's a452a8c pin
  (harness/cluster/.faultlab is outside this quest's file set).
