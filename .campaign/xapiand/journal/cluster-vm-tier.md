# cluster-vm-tier: journal

State: waiting on jobs 261118-175316-1b2e8c 261118-175317-9b20c0
Quest: .campaign/xapiand/quests/cluster-vm-tier.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

netcheck 261118-135913-76ff29 failed in the guest on nft: the one-line table needs a newline or ';'
after each set or chain's closing brace. Fixed at Xapiand:8882305c9 (netlab's one-declaration-per-line
table); net_test.py now checks nft input too: passes on the fix (261118-175312-a267a3) and fails on
the previous net.py (261118-175313-6dec6a, exit 0 when it fails). netcheck 261118-175314-8ff976 passed at Xapiand:8882305c9
(1->2 0.96 ms, shaped 53.52 ms, 1->3 unshaped 0.93 ms; partition 100% loss with 1->3 at 0%; heal 0%;
clear 2.17 ms). Queued: A1 rerun 261118-175316-1b2e8c (exclusive; guest code changed, so
e7a755 no longer counts), check suite 261118-175317-9b20c0. The earlier rerun 261118-140248-721145
was superseded.

## Next

1. When 1b2e8c and 9b20c0 pass: State "ready for review", then the correctness review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261118-115443-e7a755 (exclusive, Xapiand:d5bd31053): each of 3 guests heard both others over the hub L2; in each, the planted write was absent at `before` 5/5 and present at `after` 5/5; 0.194 s median per replay, 0.525 s max, 30 replays, log about 158 MiB. Rerun 261118-175316-1b2e8c at Xapiand:8882305c9 queued (guest net.py changed since) |

Other jobs: dev check 261118-115433-265d2c passed; park_test 261118-115436-e3d7ad (caller's group
SIGKILL, SIGTERM, guardian SIGKILL then sweep) and 261118-115437-6ea361 (keeper SIGKILL, down,
deadline) passed. netcheck 261118-115435-6123e3 failed: net.py named prio bands in decimal and tc
reads hex (1:11 is band 17 of 16); fixed in Xapiand:fb026cc9e with net_test.py, which fails on the
old net.py.

Messages: the lead's two course corrections (multicast failure, netcheck failure) are answered by
Xapiand:d5bd31053 and Xapiand:fb026cc9e and the jobs above.

## Result

`tools/vm/cluster.sh` (check, netcheck, run, up, ssh, collect, down, sweep) and
`harness/cluster/vm/` (vm.py host side; guest/disk.py, probe.py, net.py, net_test.py; park_test.sh).
How it works: `harness/cluster/vm/README.md`. A replay costs 0.194 s median (0.525 s max) over a
158 MiB log, against D14's 5 to 10 s estimate, so 60 replays take about 12 s of a run, not 10 min.
Open: the hub relays in Python, so its throughput for D14's large uploads is unmeasured; netlab's
per-peer shapes and partitions are proven by netcheck 261118-175314-8ff976.

## Rulings I made

- Guests boot overlays on document-gate's pristine `base.img` (its own `disk.qcow2` also backs on it),
  with a seed that installs nothing: python3, dmsetup, losetup and mkfs.ext4 ship in the image.
- The L2 is a learning hub in the guardian over `-netdev dgram`: `-netdev socket,mcast` cannot send
  on macOS (EADDRNOTAVAIL from a socket bound to the group).
- Seeds are cached per root and key (one instance-id per guest index; overlays are fresh each run).
- Overlays use `cache=unsafe`: deleted at park; power loss is modeled in the guest by dm-log-writes.
- One disk server per guest owns the disklab, since a lab lives as long as its opener.
- SSH ports skip 18782 and 18784 (document-gate) even when free; the L2's host UDP port is the first
  SSH port + 10000, so concurrent runs never share a wire.
- faultlab pinned at a3b71d1 in harness/cluster/vm/.faultlab, separate from run.sh's a452a8c pin
  (harness/cluster/.faultlab is outside this quest's file set).
