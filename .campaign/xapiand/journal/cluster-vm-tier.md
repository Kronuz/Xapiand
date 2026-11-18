# cluster-vm-tier: journal

State: waiting on jobs 261118-105737-42100a 261118-110246-645dd6 261118-110425-fe0bf4 261118-111122-f3c273
Quest: .campaign/xapiand/quests/cluster-vm-tier.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing has run yet: all four jobs sat queued for 15+ minutes behind a pending strict job
(A4 100K dense round 2). No VM has booted under this quest, so nothing below is proven.
Dev runs (light, not evidence): check 261118-105737-42100a, park_test 261118-110246-645dd6,
netcheck 261118-110425-fe0bf4. A1 (exclusive, 10 min, `--after` the dev check):
261118-111122-f3c273 at Xapiand:2ce58b24f.

## Next

1. Read the dev check's log (`campaign job log 261118-105737-42100a`). Likely first-boot risks:
   QEMU `-netdev socket,mcast` on macOS loopback; dmsetup or the dm-log-writes module missing
   in the bare base image (then add `apt-get install dmsetup` to provisioning); netplan's
   scope-link multicast route.
2. Fix, resubmit; A1 reruns at the fixed head (its `--after` names the old dev check).
3. Then a correctness review.

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
