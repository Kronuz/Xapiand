# cluster-harness review: correctness

Commit: Xapiand:45d76f523d7d202827e450890e36bad72f22aaf4
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Scope: the early landing of the process tier (A1, A2, A4, A5). A3 (tier 2) is not reviewed.

## Evidence I relied on

- A1: 261116-114308-647626 (done, exit 0, at Xapiand:d853aafea; Xapiand:45d76f523 differs only in
  the journal). N = 1 is GREEN on all seven items. N = 3 and N = 5 are AS EXPECTED. Every node
  answered `GET /` and saw only itself. `read`, `search`, `aggregations` and `replay` fail on
  404 "No such database" or `_doc_count` 0 on nodes 2 to 5. `failover` fails because the nodes
  do not agree on a leader. No item raised, and `history` passes. So on master the six items fail
  because there is no cluster, not because of a harness error.
- A2: 261116-113744-61bbf2 and its three `--fails-without` mutants (the `harness/cluster/` files
  are unchanged since Xapiand:7eefd4b2d). I accept that the planted histories and mutants behave
  as stated. Whether the `prefix` check is the check A2 names is F1 below.
- A4: 261116-114308-6b17b6 (met per `campaign status`). Not probed further.

## What I checked myself

Scripts are under `.scratch/review-cluster-harness-correctness/`. Every job ran at Xapiand:45d76f523.

- `zones_vs_brute.py`, job 261116-115257-9a6022: 30,000 histories compared against my own
  exhaustive search. The search uses real time plus each client's program order. The generator
  covers what the self-test's does not: integer times (ties), writes left pending all run long,
  `info` writes with a completion line, refused writes, `info`/`fail` reads, sequential clients
  with up to 3 reads, null reads, and perturbed real executions. Of the 30,000, 16,980 are
  linearizable. **0 disagreements**, except 32 histories where a client's completion and its next
  invocation share a timestamp (F3). The zone test is sound and complete for this register
  model: pending writes complete at infinity, unread optional writes are dropped, and the initial
  null works as a write at minus infinity.
- `planted.py`, job 261116-115748-a53257 (exit 1: 5 of 8 verdicts differ from the design): F1,
  F3, F4, and two control cases that come out right (a long pending write; a pending write seen
  only at `prefix`).
- `proxy_probe.py`, job 261116-115303-016b61 (exit 1): no bytes cross a partitioned link in either
  direction, but FIN and RST cross in both (F2).
- `hygiene.sh` with `fake_node.py`, job 261116-115524-c204ef (exit 1): seven exit paths of
  `run.sh check --nodes 3`. SIGTERM and SIGINT to run.sh, SIGKILL and SIGTERM to the Python side,
  a SIGSTOPped node then SIGTERM, and the CLUSTER_TIMEOUT watchdog each leave 0 nodes. SIGKILL of
  run.sh leaves all 3 nodes and checklist.py running (F5). (Job 261116-115307-c80650 is a
  superseded run of the same probe whose pattern also matched run.sh itself.)

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | The `prefix` check is not cluster-release.md D4's consistent prefix (durability-modes D10: "the state after some prefix of the one log, which every replica applies in order"). It checks per-client, per-key real-time monotonic reads, which D10 gives to `session`. It accepts a real violation: two node-pinned clients (`get@node1`, `get@node2`, as checklist.py names them) see concurrent writes x and y in opposite orders and each keeps its last, which needs two logs. It also rejects two legal histories: a client that reads a fresh replica and then a lagging one, and a follower restarted after kill -9 that serves its older snapshot. A2 calls this "the consistent-prefix check", and quest 13 A7 and R2 rest on it. | `history.py:201-218`; planted.py cases 1-3 in job 261116-115748-a53257 | Owner's choice. (a) Implement D10: per key, one write order that respects real time for completed writes, and each (node, launch) observes it monotonically; reject a cycle in the "observed before" graph. This needs the node and its launch number in every history line. (b) Narrow it: key the rule on (node, launch), call it monotonic reads per replica, state in README and A2 that consistent prefix across replicas is not checked yet, and hand that check to quest 13 / R2. Either way, planted.py cases 1-3 become self-test cases. |
| F2 | yes | defect | A partitioned link leaks FIN and RST in both directions, and every abort crosses with them. When the source sends EOF, `copy` calls `write_eof()` without waiting for `open`. On a source error it calls `transport.abort()` at once, and `forward` then closes both streams. So a node killed -9, or a connection closed by a timeout, on one side of a "silent" partition is seen at once on the other side. The README says the opposite ("silence ... nothing is reset"). This can hide the very window the harness exists to catch, such as a partitioned old leader learning it lost its peers from EOF before CheckQuorum steps it down. | `proxy.py:131-134`, `proxy.py:118-121`; job 261116-115303-016b61 (4 leaks) | Hold EOF and aborts the way bytes are held: `await self.open.wait()` before `write_eof()`, before `abort()` in the error path, and before `forward`'s final close. Add proxy_probe.py's seven cases to the self-test or to a proxy test run by `run.sh`. |
| F3 | no | defect | Same-client timestamp ties lose program order. When a client's completion and its next invocation carry equal `time`, the zone test treats the two as concurrent and accepts a non-linearizable history. Times are rounded to 1 µs (`history.py:98`). The checklist's own recorder leaves several µs between the two records, so this is unlikely there, but any other recorder can hit it. | planted.py case 5; 32 of 30,000 in job 261116-115257-9a6022 | In `load`, reject (or nudge by 1e-6) an invocation whose time is not after the same client's previous completion. Have Recorder write strictly increasing times. |
| F4 | no | defect | An `ack=none` write that a read observed is placed inside [invoke, ack]. D4 acknowledges `none` at ProposalPlaced, before commit. So "linearizable read returns null, then a later one returns x" is legal, but it is rejected. That is a false red for any history that mixes `none` writes with `linearizable` reads. | `history.py:240`, `load` keeps the ack time; planted.py case 7 | Give an observed `none` write `complete = INF`, as for `info`, and update selftest's `brute` to match. |
| F5 | no | defect | SIGKILL of run.sh orphans every node and checklist.py. The Python side runs in its own process group, which nothing kills once run.sh is gone. Campaign jobs are covered by the session sweep, and quiet sends TERM 10 s before KILL, but `timeout -s KILL`, the OOM killer or a manual `kill -9` leave the nodes running. | hygiene.sh scenario a, job 261116-115524-c204ef | Have checklist.py watch its parent: an inherited pipe whose EOF, or `getppid()` changing, triggers `kill_now()` and exit. |
| F6 | no | gap | Under `--expect-fail`, a node that never answered is not a harness error. `cluster.start()`'s `missing` is only printed. On master, a node that fails to start gives the same failed-item set as "no cluster", so the run is still AS EXPECTED. The A1 job's log shows every node answered, so A1's evidence stands. | `checklist.py:403-406` | Return exit 2 when `missing` is non-empty. |
| F7 | no | nit | A partitioned link still accepts new TCP connections at the proxy. The connect succeeds and then goes silent, unlike a DROP partition where connect times out. This is documented ("new connections wait"). A node that treats a successful connect as liveness sees a different fault. | proxy_probe.py case 7 | State it in the fault table, or refuse accepts while partitioned. |
| F8 | no | gap | kill -9, SIGSTOP, restarts and partitions are not recorded in the history. Linearizability does not need them: an op in flight at a crash completes `info` through status 0, which is correct. Any per-replica check does need them, which is F1's option (a). | `cluster.py:194-226`, `checklist.py` | Record a `launch` number per node with each operation, or a fault line the checker skips. |
| F9 | no | nit | Writes observed only by `prefix` reads are dropped from the linearizable projection, so an `info` write a prefix read saw does not have to precede a later `linearizable` null. This matches D4's scope (quorum writes and linearizable reads), and the README should say so. | planted.py case 8 | One README sentence. |

## Not run

- Tier 2 (A3, `tools/vm/cluster.sh`): not built at this commit.
- Faults against real `xapiand` nodes: master has no cluster, so partitions, SIGSTOP and kill -9
  were probed only through `proxy.Link` and fake nodes. The mesh was not checked as a real
  cluster's transport.
- A4's bandwidth figures: I took the job's result as given.
- `selftest.py` was not rerun: the A2 job at unchanged harness files covers it.
- CI wiring and build-test/e2e checks: CI's, still queued at the time of this review.
