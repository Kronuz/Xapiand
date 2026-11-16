# cluster-vm-tier: The VM tier of the cluster harness

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status cluster-vm-tier`.

## Outcome

Tier 2 of `docs/design/cluster-release.md` D14, split from cluster-harness once its process tier
landed: 3 to 5 Linux VMs under QEMU on one L2, SSH on ports 18780 and up, a `dm-log-writes` device
under the data directory for power loss, driven by `tools/vm/cluster.sh` (up, run, collect, down;
every VM parked on every exit). Per D95 this tier is built as faultlab's disklab and netlab once
that library exists; until then, nothing here starts.

## Acceptance criteria

- A1 Three VMs boot on one L2 that carries multicast between them (a probe datagram from each guest
  received by the other two), and a `dm-log-writes` replay of a planted unsynced write finds it
  missing at the mark before its flush and present at the mark after; the job states the seconds
  per replay, which sizes D14's 20-run power-loss tier (an `exclusive` job)

## Decisions

- Lead: Detent's VMs on 18762 and 18763 and strata's on 18772 and 18774 are never touched. No AWS.
- Lead (D95): built in Kronuz/faultlab's Linux labs, not in Xapiand's tree.

## May land early

None.
