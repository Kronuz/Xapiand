# faultlab-netlab: journal

State: waiting on jobs 261118-064359-d81166 261118-064401-e15efd 261118-064403-31cd75 261118-064404-807b9c
Quest: .campaign/xapiand/quests/faultlab-netlab.md (the lead's; never edit it)

## Doing

Jobs at faultlab:b762f1c: A2 test and its mutant (refs/mutants/netlab-a2-delay-event, faultlab:6bc239e,
delay writes no event), A1+A3 live run in the VM, build-test check.

## Next

1. Paste the evidence table when the jobs finish; then ready for review.

## Blocked on

The lead, for CI (A4):
- netlab_test.py is not registered with ctest: CMakeLists.txt is outside the file set. Recommend
  adding `netlab_test` to the `foreach(t selftest proxy_test scenario_test)` list (one word). On
  macOS, FreeBSD and non-root Linux it runs the recording-lab half (A2) and prints
  `SKIP live probes and teardown: <reason>`; `--live` makes the skip a failure.
- CI on the quest branch needs a push, which contributors do not do.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261118-064403-31cd75 (first run 261118-064217-267405: every A1 row ok) |
| A2 | 261118-064359-d81166; mutant 261118-064401-e15efd |
| A3 | 261118-064403-31cd75 |
| A4 | build-test 261118-064404-807b9c; CI: see Blocked on |

First live run (faultlab:dac334f, Ubuntu 24.04, kernel 6.8, in the VM): delay 50 ms each way adds
104.3 ms to a round trip (base 0.37); loss 10% loses 9.35% of 10,000; duplicate 10% duplicates
10.26%; reorder 25% (delay 50) makes 18.61% overtake their predecessor (p(1-p) = 18.75), delay
alone 0; cap 20 Mb/s gives 19.13 Mb/s TCP goodput (1448/1514 of the wire); partition, isolate,
heal and reset all as specified. Teardown: exit, SIGTERM, SIGHUP, SIGINT clean; SIGKILL leaves 3
namespaces that sweep removes; listing after equals before. Its one failure was the test racing
`close` against its own count, fixed in b762f1c.

## Result

faultlab `python/faultlab/netlab.py`: a namespace per node, a veth pair per pair of nodes created
inside the two namespaces, netem per veth end, an nftables `cut` set per namespace. Mesh's
interface (`start`, `address`, `partition`, `heal`, `isolate`, `reset`, `cap`, `close`) plus
`delay`, `loss`, `reorder`, `duplicate`, each optionally on `a`'s links or the pair (a, b).
Docs: `docs/api.md` (netlab.py), `docs/mechanisms/netlab.md`, README. Test:
`python/tests/netlab_test.py [--live]`.

## Rulings I made

- reset uses `ss -K` (kernel socket destroy, which sends the peer a RST) on both sides, not the
  design's nftables `reject with tcp reset`: a reject rule reaches an idle connection only when it
  next sends, and would refuse new connections until removed; `ss -K` is immediate and one-shot,
  as Mesh.reset is. netlab verifies no socket survived and raises otherwise (needs
  CONFIG_INET_DIAG_DESTROY, which Ubuntu sets). F14's text in cpp-libraries still names reject;
  the lead may want it updated.
- partition drops at the receiver's input (nft set of ifnames, one element per directed pair), so
  the sender sees silence and TCP retransmits after heal, matching the proxy's hold-and-deliver.
- Shaping calls take optional `a`, `b` (all links, a's, the pair), a superset of Mesh.cap(mbit);
  their events add `nodes` only when selected. `delay` is per direction. `p` is a probability 0-1.
- `reorder` without a delay raises ValueError (tc refuses it too; netem reorders by skipping the
  delay).
- Teardown traps SIGTERM/SIGHUP only where the handler is the default; SIGINT exits via atexit;
  SIGKILL is covered by pid-named namespaces swept by the next lab or `netlab.py sweep`.
- Requires root; the user-namespace route is not built (the VM runs it as root).
- VM jobs run through `~/.scratch/faultlab-netlab/vmjob.sh`, which starts the 18782 VM only when
  stopped and stops only what it started.
