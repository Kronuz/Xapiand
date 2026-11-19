# cluster-vm-tier review: correctness

Commit: Xapiand:2d4a2e7b2349860eabbe36d9f431dcb31523cfbc (code identical to Xapiand:8882305c9; the later commits touch only the journal)
Verdict: APPROVE
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- netcheck 261118-175314-8ff976 at Xapiand:8882305c9: 50 ms on 1->2 (53.52 ms), 1->3 unshaped, partition 1|2 at 100% loss with 1->3 at 0%, heal, clear.
- park_test 261118-115436-e3d7ad and 261118-115437-6ea361 at Xapiand:d5bd31053 (caller's group SIGKILL, SIGTERM, guardian SIGKILL then sweep, keeper SIGKILL, down, deadline). They still count: `vm.py` and `cluster.sh` have not changed since d5bd31053 (`git diff --stat d5bd31053 8882305c9` touches only net.py, net_test.py and the journal).
- net_test.py passing on the fix (261118-175312-a267a3) and failing on the old net.py (261118-175313-6dec6a).
- Check suite 261118-175317-9b20c0 at Xapiand:8882305c9: done, exit 0.
- A1 rerun 261118-175316-1b2e8c was still queued, so A1 is not judged here (see Not run).

## What I checked myself

Scripts are in `.scratch/xapiand/review-cluster-vm-tier-correctness/`, with rerun commands in README.txt.

- **l2probe 261118-180516-3eaa57** (`l2probe.sh`; 3 guests via `cluster.sh run`). It exited 1 only on the multicast lines and the partial-setup line. The multicast lines failed from a probe bug: JSON keys are strings and the probe compared them to ints. That is fixed in the script since; the data below is from that run's log.
  - **Q1, isolation.** Full 6-pair ping matrix plus a multicast "heard" matrix:
    - Baseline: all pairs connected.
    - Partition 1|2 on both sides: only 1->2 and 2->1 are lost. 1<->3 and 2<->3 stay at 0%. Node 3 hears both others; 1 and 2 hear only 3.
    - Heal: everything is restored.
    - Partition on node 1 only: both pings fail, since the replies are dropped. Node 2 still hears node 1's multicast and node 1 does not hear node 2's, which is the documented asymmetric cut.
    - `clear` without a heal: `nft list tables` is empty, the root qdisc is back to fq_codel, there are no u32 filters, and the matrix is clean.
  - **Q2, shaping.** Root ping, 40 packets at 0.05 s intervals. The unshaped minimum was 0.81 ms.

    | Configured | min | avg | Over configured + base (min / avg) |
    |---|---|---|---|
    | 10 ms | 10.98 | 11.81 | +0.17 / +0.25 ms |
    | 50 ms | 51.02 | 51.83 | +0.20 / +0.27 ms |
    | 100 ms | 100.93 | 101.95 | +0.11 / +0.38 ms |

    1->3 stayed at about 1 ms throughout. So the delay is the configured one, with at most 0.4 ms from the host. netcheck's 53.52 ms is ping's average at 0.2 s intervals and includes host jitter: the unshaped max in the same probe was 17 ms.
  - **Q2, hex bands in the kernel.** I gave peers 9, 10 and 15 permanent neighbor entries, shaped them to 9, 10 and 15 ms, and sent 7, 11 and 13 pings. `tc -s qdisc` showed each count in exactly the right leaf, and no other peer leaf carried a packet:
    - leaf `19:` under `1:a`, delay 9ms, 7 packets
    - leaf `1a:` under `1:b`, delay 10ms, 11 packets
    - leaf `1f:` under `1:10`, delay 15ms, 13 packets
  - **Q4, the hub.** All three guests sent sequenced multicast at once:
    - Paced, 2000 each at 1 ms: 2000/2000 from each sender at every receiver, 0 duplicates, 0 reordered.
    - Burst, 5000 each back to back: 0 duplicates and 0 reordered. Loss was 92, 97 and 1285 per receiver, and each guest's `Udp RcvbufErrors` delta matched its missing count exactly (92, 97, 1285). All the loss was in the receiving guest's socket buffer and none in the hub.

    The hub floods group, broadcast and unknown-destination frames to every other link, as a switch without IGMP snooping does.
- **Q3, timeout probe 261118-180518-7a0f1a** (`timeout.sh`: `cluster.sh run --nodes 2 -- sleep 900` with `--minutes 2`). The job reached its deadline (exit 124, 125 s). `check_timeout.sh` on run-261118-180759-8743 then found:
  - `state.json` parked, reason SIGTERM (the EXIT trap's `down`), written at 18:10:02 within the stop window.
  - No `node*.qcow2` left.
  - No process whose command line names the run.
- By reading:
  - The SSH range is 18780-18799 minus 18782/18784, so it never reaches Detent's or strata's ports (`vm.py:59-60`).
  - The guardian, keepers and QEMU run in their own session (`vm.py:522-528`), so the job runner's session sweep cannot orphan them. Owner death parks the run within about 1 s (`vm.py:276-278`).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | defect | `setup()` treats an existing prio root as complete (`net.py:70-71`). After a setup that stops partway (tc `-batch` stops at its first error; or tc succeeds and nft fails, as in netcheck 261118-135913-76ff29), the next `shape PEER` whose filter was never added exits 0 but shapes nothing. `partition` does fail loudly. Nothing outlives a run, because guests boot fresh overlays. | l2probe 3eaa57: with only a prio root in place, `shape 1 --ms 50` printed nothing and exited 0, and 3->1 was 1.28 ms. After `clear`, the same shape gave 52.30 ms. | Make setup all-or-nothing: on any failure run `clear`'s two deletes and re-raise. Optionally also require the nft table to exist before skipping. Add a net_test.py case where `run` raises on the nft step and the next command re-runs the whole setup. |
| N2 | no | gap | Partition and shape match IPv4 only: `ip saddr @cut` (`net.py:84`) and `protocol ip` u32 filters (`net.py:78`). IPv6 on `l2`, such as link-local addresses, which netplan enables by default, passes both. netlab itself cuts by `iifname`, which covers every protocol. | Read only; not probed. | Disable IPv6 on `l2` in the seed's network-config (`link-local: []`), or add `ip6` matches. Alternatively, state that the L2 is IPv4-only in README.md. |
| N3 | no | nit | The rate, loss, reorder and duplicate settings of a shape never apply to multicast, because filters match unicast destinations only and multicast stays in band 1. A lossy 1->2 shape still delivers node 1's discovery multicast to node 2. README.md documents this. | `net.py:72-79`, README.md "Network faults" | Owner's call. Keep it, or shape at ingress (an ifb device per guest) if discovery under loss matters. |
| N4 | no | nit | `heal PEER` for a peer that is not cut exits 1 (nft "No such file or directory"), so heal is not idempotent. | l2probe 3eaa57, info line | Ignore the missing-element error, or flush-and-re-add. |
| N5 | no | nit | netcheck's partition check pings only 1->2 and 1->3. It would pass even if node 2's rule did nothing, and it never checks 2<->3. | `vm.py:677-681`; l2probe covers both | Optionally add 2->3 and a one-sided case to netcheck. |
| N6 | no | nit | An `up` killed before the guardian writes `state.json` leaves the run directory's overlays in place. `sweep` skips runs without a state file. | `vm.py:157-160`, `vm.py:506-517` | Let sweep also discard the disks of runs with no state whose owner pid is dead. |
| N7 | no | nit | The hub thread returns on any `recvfrom` OSError and the guardian never notices (`vm.py:209-213`), so the L2 would go dark without parking. No such error was seen. | Read only | Have the guardian loop check that the relay thread is alive and raise Stop if it is not. |

## Not run

- A1 itself (exclusive rerun 261118-175316-1b2e8c was queued throughout). The dm-log-writes replay and the seconds-per-replay number are not reviewed here.
- Hub throughput for large uploads (the journal lists it as open).
- Linux hosts: every probe ran on this macOS host with hvf.
- The IPv6 behavior in N2 (read, not probed).
- A timeout that lands during `up`, as opposed to during CMD. The park_test group-kill case and owner-file handover (`vm.py:507`, `vm.py:543`) cover it by reading only.
