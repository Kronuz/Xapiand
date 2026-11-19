# The cluster harness's VM tier

Tier 2 of `docs/design/cluster-release.md` D14: 3 to 5 Linux guests under QEMU on one L2 that
carries multicast, each with its data directory on a `dm-log-writes` device, for the cluster's
multicast discovery and for power loss. It runs as jobs under quiet holds, never in CI.

```sh
tools/vm/cluster.sh check [--nodes 3]                 # A1: multicast between every pair, a planted write replayed
tools/vm/cluster.sh run [--nodes N] [--out DIR] -- CMD # up, CMD with CLUSTER_VM_RUN set, collect, down
export CLUSTER_VM_RUN=$(tools/vm/cluster.sh up --nodes 3)
tools/vm/cluster.sh ssh 1 'python3 -I /opt/cluster/guest/disk.py ping'
tools/vm/cluster.sh collect build/cluster-vm/mine /var/log/syslog
tools/vm/cluster.sh down
```

## How it works

```
host                                      guest i (Ubuntu cloud image, user `cluster`)
QEMU i  -netdev user   127.0.0.1:1878x -> wan   DHCP, SSH
        -netdev dgram 127.0.0.1:L <-> hub   <-> l2  10.87.0.i/24, route 224.0.0.0/4
        node<i>.qcow2 (backing: base image)     /srv/xapiand -> disklab mount (dm-log-writes)
```

**Guests.** Each guest boots a qcow2 overlay whose backing file is the base cloud image
(`CLUSTER_VM_BASE`, default document-gate's `~/.scratch/vm-document-gate/linux/base.img`), so a guest
costs only the blocks it writes, and a cloud-init seed (shared, read-only) that names it `node<i>`, adds the `cluster` user
with the run root's `ssh_key`, and installs nothing. Overlays use `cache=unsafe`: they are deleted at
park, and power loss is modeled inside the guest, not by the host's cache.

**The L2.** Each guest's second NIC is a QEMU `-netdev dgram` whose frames go, one per UDP datagram,
to a hub in the guardian on `127.0.0.1` (port: the run's first SSH port + 10,000; guest i's end: its
SSH port + 11,000, so concurrent runs never share a wire). The hub learns each source MAC's link and
sends a frame to the link of its destination, or, for broadcast, multicast and unknown destinations,
to every link but its own, as a switch does, so the guests share one L2 that carries multicast. A
datagram from an address that is no guest's link is dropped. QEMU's `-netdev socket,mcast` cannot
serve here: it sends from a socket bound to the group address, which macOS refuses
(`EADDRNOTAVAIL`), so no frame ever leaves. Guest i is `10.87.0.i`, with `224.0.0.0/4` routed to the
L2. SSH forwards take the free ports from 18780 up to 18799, skipping 18782 and 18784
(document-gate's VMs); Detent's (18762, 18763) and strata's (18772, 18774) are outside the range.
The cloud-init seeds are built once per root and key under `CLUSTER_VM_ROOT/seeds`, since `hdiutil`
can take minutes on a loaded host and every run boots fresh overlays.

**Data directory.** `up` copies `guest/` and the pinned faultlab's Python to `/opt/cluster` and starts
`guest/disk.py serve` under `systemd-run` (unit `cluster-disk`): it opens a faultlab disklab (512 MiB
by default, `--disk-mb`), links `/srv/xapiand` to its filesystem, and answers `mark`, `stop`,
`replay`, `unreplay` and `points` on `/run/cluster-disk.sock`, because a lab lives as long as the
process that opened it. A power cut is `stop` (unmount; the log ends) and then `replay TO`, which
mounts the state the cut leaves at a mark or entry; faultlab's `docs/mechanisms/disklab.md` has the
log and replay mechanics.

**Network faults.** `guest/net.py` builds faultlab netlab's `Shape` into the guest's `l2` egress: a
prio qdisc with one band per peer, a frame to peer j's MAC (`52:54:00:87:01:j`) sent by a u32 filter to
a band whose netem carries that direction's delay, loss, reorder, duplicate and rate. A partition adds
the peer's MAC to an nftables set whose input rule (`ether saddr`) drops it, as netlab drops where a
packet arrives, so TCP retransmits after heal; run it on both guests to cut both directions. Peers are
matched by MAC, so IPv4, IPv6 (link-local included) and every other protocol obey. The nftables table
is built last and marks a complete setup: a command that finds it missing clears the guest's tc and
nft state and builds it all again, so a setup stopped partway is redone, never skipped. `heal` of a
peer that is not cut exits 0.
`tools/vm/cluster.sh netcheck` proves, over IPv4 and IPv6, a 50 ms shape on 1 to 2 and not on 1 to 3,
and a partition of 1 and 2 that leaves 1 and 3 and 2 and 3 talking until heal. `guest/net_test.py`
runs on any host: it checks every class and nft block the commands name, and replays interrupted
setups against a model kernel.

Known limits. Broadcast and multicast frames stay in the unshaped band, so a shape's loss, reorder and
duplicate never touch discovery multicast; a partition does cut it, on the guest that has the cut. The
L2's hub relays in Python and floods group, broadcast and unknown-destination frames to every link, as a
switch without IGMP snooping does; a burst of 5,000 datagrams from three guests at once lost 2 to 26%
in the receivers' socket buffers and nothing in the hub (review of 2026-11-18). The guardian parks the
run when the hub thread dies. A run whose `up` was killed before the guardian wrote `state.json` loses
its overlays at the next `sweep`, once its owner is dead.

```sh
tools/vm/cluster.sh ssh 1 sudo python3 -I /opt/cluster/guest/net.py shape 2 --ms 50 --loss 0.01
tools/vm/cluster.sh ssh 1 sudo python3 -I /opt/cluster/guest/net.py partition 2   # and on 2: partition 1
tools/vm/cluster.sh ssh 1 sudo python3 -I /opt/cluster/guest/net.py heal
```

**Parking.** `up` starts a guardian (`vm.py guard`) in its own session; it starts one keeper per guest
(`vm.py keep`), and each keeper runs one QEMU and stops it when the guardian's pipe closes. The guardian
closes every pipe, waits for the keepers, deletes the overlays and records the reason in
`state.json` when the owner pid dies (`check` and `run` name their shell), at the deadline
(`CLUSTER_TIMEOUT`, 1,800 s by default), on SIGTERM, SIGINT or SIGHUP (`down`), or when any keeper or
guest stops. Killing the caller, even with SIGKILL, parks the run within a second; a SIGKILLed guardian
closes its pipes, so the keepers stop their guests; a killed keeper is seen by the guardian; and
`sweep`, which every `up` runs first, SIGKILLs what a dead guardian left by its recorded pids, checking
each pid's command line names the run. Nothing is killed by name pattern. Runs live under
`CLUSTER_VM_ROOT` (default `~/.scratch/cluster-vm`), outside any checkout, so a sweep sees them all;
a parked run keeps only its logs and `state.json`.

**check (A1).** Every guest runs `guest/probe.py` at once: it sends its name to `239.192.87.1:5870`
from its L2 address every 0.2 s for 10 s and lists the names it hears and from where; each must hear
every other guest from its L2 address. Then each guest, in turn, writes 128 MiB of synced files (a log
the size of a short run), writes 64 KiB without syncing, marks `before`, fsyncs, marks `after`, stops
the lab, and replays each mark 5 times: the planted file must not be present at `before` and must be
present at `after`. Each replay's seconds (rebuild, mount with journal recovery, read and digest,
unmount) are printed, with the median and the maximum, and written to `summary.json` in the output
directory.
