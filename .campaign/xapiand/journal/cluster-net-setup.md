# cluster-net-setup: journal

State: waiting on jobs 261118-202125-ad447b 261118-202126-c9a5a2
Quest: .campaign/xapiand/quests/cluster-net-setup.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead's msg: l2probe 5a7494 FAILed "partial setup" lines. Diagnosis from its log (not a defect): the probe
expected the old behavior (`partition` fails on a prio-only guest). Now the setup is redone, so
`partition 1` on node 3 succeeded (empty output, exit 0) and really cut node 1 at node 3; the next
step's `shape 1` then ran with that cut still in place, so 3->1 was inf (a cut, not a broken shape).
Rewrote the probe step (~/.scratch/xapiand/cluster-net-setup/l2probe.py): each partial case starts from
a prio-only guest and is judged by effect (partition: 3->1 loss 100% then heal restores; shape: rtt
50-60 ms). Rerun: l2probe 261118-202125-ad447b, netcheck 261118-202126-c9a5a2.

## Next

1. When both finish: if ad447b passes, State "ready for review"; else fix what its log shows.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | fails-without 261118-194233-3dc361 (net_test.py at Xapiand:fbe0aa24a, old net.py); passing 261118-194500-d0da34 at Xapiand:3a0187e2b |
| A2 | netcheck 261118-194503-0ec526 (IPv4 and IPv6 values); l2probe 261118-194506-5a7494 |
| A3 | suite 261118-194512-0404ca, CI on the pushed branch; park_test 261118-194509-6f88ff |

## Result

N1: the nftables table is built last and marks a complete setup; a command finding it missing clears
and redoes everything (net.py). N2: peers are matched by MAC (u32 at offsets -14/-12 for egress, nft
`ether saddr` set for input), so IPv6 and every protocol obey. N4: heal is add-then-delete in one nft
transaction. N5: netcheck adds 2->3 and a repeated heal. N6: sweep deletes overlays of stateless runs
with a dead owner (sweep_test.py). N7: the guardian parks when the hub thread dies. N3 (multicast
unshaped) is documented in harness/cluster/vm/README.md with the other limits.

## Rulings I made

- MAC matching instead of ip6 addresses: the guests' link-local address generation is not ours to
  pin, and one match covers every protocol (netlab cuts by interface for the same reason).
- Setup marker is the nftables table (last step) rather than rollback on failure: also covers a
  setup killed mid-way, which no except clause can.
- N3 kept as documented; ARP and ND unicast to a shaped peer are delayed with it.
