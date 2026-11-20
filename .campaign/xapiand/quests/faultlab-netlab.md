# faultlab-netlab: netlab: Linux netns per node, netem delay/loss/reorder/duplicate/rate, nftables partition and reset (design F14)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-netlab`.

## Outcome

netlab, the Linux network tier of faultlab (F14 in cpp-libraries `docs/design/faultlab.md`): a network namespace per node joined by veth pairs; netem on each link for `delay`, `loss`, `reorder`, `duplicate` and rate (`cap`); nftables per directed pair for `partition` (drop) and `reset` (TCP reset). It has the proxy tier's interface (`partition`, `heal`, `isolate`, `reset`, `cap`), and each call writes its fault event in the history log (F12). Runs as root or in a user namespace on Linux, so it runs in our Linux VM (~/.scratch/vm-document-gate, port 18782) as jobs; macOS keeps the proxy tier. This is what lets the cluster be tested over a real, lossy network rather than loopback.

## Acceptance criteria

- A1 Each fault is observable: in the VM, a probe between two namespaces measures the added delay (±10%), the loss rate over 10,000 packets (±2 points), reordering and duplication counts, and the rate cap; `partition` drops, `reset` resets, `heal` restores: a job
- A2 Every call writes its fault event, and history.py reads a log that mixes operations and netlab faults: a test
- A3 Teardown leaves no namespace, veth or nftables rule behind, also after the harness is killed: a job listing them before and after
- A4 The build-test check passes, and CI is green (netlab's tests skip on macOS with a named reason, never silently)

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
