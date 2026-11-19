# cluster-net-setup: journal

State: waiting on jobs 261118-194233-3dc361 261118-194500-d0da34 261118-194503-0ec526 261118-194506-5a7494 261118-194509-6f88ff 261118-194512-0404ca
Quest: .campaign/xapiand/quests/cluster-net-setup.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

All fixes committed and pushed (Xapiand:fbe0aa24a test only, Xapiand:37cacbd1e net.py, Xapiand:3a0187e2b
vm.py/README/sweep_test). Waiting on the jobs below.

## Next

1. When the jobs finish: read the netcheck log (IPv6 values), l2probe (partial setup line), fails-without
   3dc361 (must show the FAIL lines, not a fetch error), park_test, suite. If the u32 `at -14` MAC
   filter or `ether saddr` is refused by the guest, fall back to flower `dst_mac` for tc.
2. Fill Evidence and Result, State "ready for review".

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
