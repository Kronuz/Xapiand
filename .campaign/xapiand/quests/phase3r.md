# phase3r: Phase 3R, replication groups

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status phase3r`.

## Outcome

`strata::group` replicates strata indexes: journal checkpoints reference strata manifests, consistency
modes are chosen per index, documents replicate, replicas repair and a scrubber finds damage. Under
simulated crashes, power loss at every barrier, partitions, clock skew, membership changes and
corruption of every file class, no quorum-acknowledged write is lost, no corruption is served, and a
replica rebuilds after disk loss; quorum-acknowledged ingest is measured against Elasticsearch.

## Acceptance criteria

From strata `docs/PLAN.md`, "Phase 3R: replication groups": its item and gate. It starts after Track
C's C1 to C4 pass and Phase 3's lifecycle rows pass (they did: `phase3-lifecycle`).

- A1 `strata::group`: journal checkpoints reference strata manifests; consistency modes; document-mode replication; repair; a scrubber (strata `docs/design/durability-modes.md`, Part A): tests in jobs
- A2 Gate: at least 10^7 simulated steps over at least 1,000 seeds each for 3 and 5 voters, with crashes, power loss at every barrier (journal, segment, manifest), partitions, clock skew, membership changes and corruption of every file class: simulator jobs
- A3 Gate: the linearizability checker passes over write and get-by-id histories: simulator jobs
- A4 Gate: zero quorum-acknowledged loss; zero served corruption; replica rebuild after disk loss: jobs
- A5 Gate: quorum-ack ingest against Elasticsearch 8.x with one replica, one sequential client and 16 concurrent clients; p99 acknowledgement latency on the bare-metal machine's NVMe (`F_FULLFSYNC` on macOS): exclusive jobs on the bare-metal host
- A6 Gate: fsyncs per second per node at 1,000 groups within budget: exclusive jobs
- A7 Every gate: ASan, UBSan and TSan; one CPU-hour of fuzzing per new decoder; fixtures; coverage; the component's behavior at the target cluster scale stated: jobs
- A8 Before this gate: Phase 3's rows on FreeBSD and arm64 (`phase3-ratchets` A8) and the document profile gate's arm64 rows (`document-gate`): the quests' landings

## Contracts

Consumes Track C's `consensus` and `journal` (`track-c`) and strata's manifests (the manifest log, if it
lands first: `manifest-log`); provides the group that Phase 7b puts under Xapiand's shards.

## Decisions

- Parked by the owner (2026-10-20, D4) and after the single-node release (2026-10-29, D9). Blocked
  until Track C's C1 to C4 pass and the owner resumes distribution.
- Adopts `design/durability-modes` (strata:95357d975: the design at revision 10, approved with
  conditions, with its model, schedules and results under `bench/durability/`; 722 commits behind
  strata main); reviews `docs/reviews/durability-modes-design.md` and `-security.md` on main.
- Open (owner): the bare-metal host for A5 and A6 (PLAN open question 9).

## May land early

The design and its model.
