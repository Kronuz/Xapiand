# cluster-vm-tier: journal

State: ready for review
Quest: .campaign/xapiand/quests/cluster-vm-tier.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Ready for review. Queued at Xapiand:fb026cc9e: net_test 261118-135912-244d58, net_test on the
pre-fix net.py (exit 0 when it fails) 261118-135921-f6d2e5, netcheck 261118-135913-76ff29, check
suite 261118-135922-9079eb; A1 rerun at the head (guest/net.py changed, so the guests' code did).

## Next

1. Correctness review (the lead's).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261118-115443-e7a755 (exclusive, Xapiand:d5bd31053): each of 3 guests heard both others over the hub L2; in each, the planted write was absent at `before` 5/5 and present at `after` 5/5; 0.194 s median per replay, 0.525 s max, 30 replays, log about 158 MiB. Rerun at the head queued (net.py changed since) |

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
per-peer shapes are proven by netcheck only once 261118-135913-76ff29 passes.

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
