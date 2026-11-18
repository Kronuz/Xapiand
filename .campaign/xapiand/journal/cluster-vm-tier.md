# cluster-vm-tier: journal

State: waiting on jobs 261118-115433-265d2c 261118-115435-6123e3 261118-115436-e3d7ad 261118-115437-6ea361 261118-115443-e7a755
Quest: .campaign/xapiand/quests/cluster-vm-tier.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

First runs (lead's msg, jobs 261118-105737-42100a, -110425-fe0bf4, -110246-645dd6):
- Replay works: in all 3 guests the planted write was absent at `before` (5/5) and present at
  `after` (5/5), log about 158 MiB, 0.343 s median per replay (max 0.993, 30 replays; light job,
  host at load 600+, not the A1 timing).
- Multicast failed: no guest heard any other. Cause, reproduced on the host: QEMU's
  `-netdev socket,mcast` sends from a socket bound to the group address, and macOS refuses that
  sendto with EADDRNOTAVAIL (`~/.scratch/cluster-vm-mcast/t.py`). Fix: `-netdev dgram` per guest
  to a learning hub in the guardian (host unit check: flood, learned unicast, stranger dropped).
- netcheck crashed parsing "100% packet loss"; it now parses with a regex.
- park_test: 4 cases passed (caller's group SIGKILL, SIGTERM with journal collected, guardian
  SIGKILL, then sweep), each parked within 1 s. keeper_kill's `up` then spent minutes in hdiutil
  at load 600+ until the 30-min deadline. Seeds are now cached per root and key, hdiutil, qemu-img
  and the guardian start are bounded, and the cases run as two jobs.

Queued at Xapiand:d5bd31053: dev check 261118-115433-265d2c, netcheck 261118-115435-6123e3,
park_test 261118-115436-e3d7ad and 261118-115437-6ea361, A1 exclusive 261118-115443-e7a755
(after the dev check).

## Next

1. Read the five jobs. If multicast still misses, check's output now has each guest's
   `ip -br addr`, route and l2 counters.
2. A1 green, then a correctness review.

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
