# placement review: correctness

Commit: ae70ff95 (strata:ae70ff95f8908be6a18f28af5ca35401eed44109, quest/xapiand/placement; reviewed diff a1e786f4..ae70ff95)
Verdict: CHANGES
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A1: 261123-033244-fcdb6d (5 root ctests at ae70ff95), fuzz 261121-213405-46539a, root mutants
  261120-184058-c79fd0 .. 261120-184144-06d7d2. These cover the records, the placement property and the
  minority-loss simulation; I did not rerun them.
- A2: 261123-033402-76ca9b (20,000 model sets, 19,879 bound cases, worst 0.98 of subtree + depth + 1),
  mutant 261120-181211-11c0e5.
- A3: 261123-033248-aed250 (plan, 3,000 seeds), -033256-4f8bcc, -033300-366693, -033304-7f8260,
  -033307-d5ae57, -033311-0f4e28 (live groups); the repair mutants listed in `campaign brief`.
- A4: the defer case, 261123-033252-6cab6f (3 seeds); mutants 261121-002729-8d49af,
  261121-004909-30fad7.
- I also checked by reading: the root's prepare runs after the ready point (`submit.hh`, past the
  term's first entry), with only this leader's own pending commands in the overlay, and every
  command is conditional on versions (`index.hh:4915`, 0 means absent).

## What I checked myself

All probes are in `.scratch/xapiand/review-placement/` (`all.sh` runs them all; `run.sh` exports
ae70ff95 into `work/src` and leaves the contributor's worktree alone). One job ran them all:
**261123-045604-845e36** (exit 0; each result is printed in its log).

1. `probes/orphan.inc` (inserted into repair_plan_test): repair adds a learner on a free node.
   That node is removed before the next pass records the learner. Then every pass plans the same
   `record` step, and `record_shard` refuses it every time.
2. `probes/pileup.inc`: 8 nodes, 24 namespaces at rf 3, and node 1 lost. One `plan_all` pass adds
   6 learners. 5 go to n3, 1 goes to n8, and no other node gets one.
3. `probes/scan_churn.inc` (inserted into scan_test): 20,000 scans run against a source that
   creates and deletes records between calls. Every matched record that was present for the whole
   scan is listed exactly once, nothing is listed twice, and the scan never moves backward. It
   passes.
4. Mutant `mutants/pass-no-count.py` removes the in-pass `count()` in `plan_all`. It survives the
   plan test (3,000 seeds) and the live group test (every case, 1 seed).
5. Mutant `mutants/candidate-down.py` lets a down-but-not-lost node take a new learner. It survives
   both tests.

Answers to the lead's questions:

- **Root (Q1).** No range ever has two owners or none. There are no ranges and no splits here.
  Ownership is namespace record, then shards, then group ids. Group ids come from `G`, which is
  written with a version check. Every record change is one versioned transaction in one
  strict-consensus group, prepared after the leader's ready point. A stale prepare fails with
  `version_conflict`, and the burst test kills the overlay-version mutant (`placements == 1`).
  What a reader can observe is committed state.
- **Scan (Q2).** The scan moves only forward, and a record is read at most once. Probe 3 shows
  exactly-once for stable records while records are created and deleted. A "move" rewrites a
  record's value, never its key, so it cannot reorder the scan. The quest has no split. F4 covers
  the missing source over the real index.
- **Repair (Q3).** Membership changes have no compare-and-swap. A stale `next` is refused by
  consensus's single-step rule and the incarnation counter, or it repeats the step already made.
  Removals keep a quorum of up voters (`valid()`, killed by the `no-quorum-guard` mutant). The
  defer test checks values: same incarnation and term while the upload is open, the upload's 6
  bytes on every member, and the leader on the preferred node afterwards. Two stale-view defects
  remain: F1 (load lags membership) and F2 (loads are not counted within a pass).
- **Tests (Q4).** The two weakest tests are the `plan_all` pass loop and candidate choice. Both
  mutants survive both repair tests (F2, F3).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Repair can stall a shard for good. `remove_node` checks only the recorded load (`root.hh:250`), and the record trails the membership by one pass. So a node that has just taken an unrecorded learner (load 0) can be drained and removed, or just removed. After that, `plan` always returns `record` first (`repair.hh:286-293`). `replace_namespace` refuses a member with no load as `invalid` (`root.hh:288`). Every pass replans the refused record, the lost learner is never removed (step 1 is never reached), and every later replacement in that group is blocked. | job 261123-045604-845e36, probe `orphan`: "removed ... accepted; pass 1..3: record -> record_shard refused (3)" | Before recording, plan a removal of members on nodes the root does not know, or record only known nodes. Optionally refuse `remove_node`/`drained` while a group report still names the node. Test: the orphan scenario as a plan case that expects the learner removed and then a record accepted. |
| F2 | yes | defect | Within a pass, `plan_all` counts only `record` steps (`repair.hh:475`). Learners it adds are not counted, so every shard of a lost or drained node picks from the same unchanged loads, and the new replicas pile onto a few nodes. Nothing ever rebalances group counts. At the owner's scale, one dead node's whole re-replication lands on one or two nodes. `placement-repair.md:42` says loads count before the next shard plans. | probe `pileup`: 6 new learners, 5 on n3 and 1 on n8; mutant `pass-no-count` survives the plan and group tests (same job) | Count `joining` into `nodes` (groups +1) for change steps in `plan_all`. Test: a `plan_all` pass over at least 20 shards with one node lost, asserting the learners spread (no node gets more than one above the fewest-groups choice). |
| F3 | no | gap | No test covers the candidate filter that skips down-but-not-lost nodes (`repair.hh:319`). The property test marks nodes down and lost together. Without the filter, a learner joins a down node and its shard waits until `replace_after`. The same thing happens after a root leader change, because the keepalive table starts empty ("a node absent is up"). | mutant `candidate-down` survives both tests (same job) | Add a plan case with a free node that is down but not lost, expecting a learner on another node. |
| F4 | no | gap | A2 is proven only against a vector source (`scan_test.cc:55`). No `scan_source` over the root's index exists in the quest. The mechanism note says it would start a new `basic_index::scan` per seek (`placement-root.md`, "What the scan relies on"). The work bound on the real index is therefore the consumer's to keep. | read; probe `scan_churn` covers the live-source semantics | Either add the adapter here with a test, or name it in `strata:placement-records` for the cluster quest. |
| F5 | no | nit | A drain or loss with no free node shrinks shards below rf instead of waiting. Draining 2 of 3 nodes leaves rf 3 groups as one voter (`repair.hh:298-299`, `target`). This is documented and follows D7, so it is the owner's call. Options: keep it; or refuse or hold a drain that would take a shard below rf (or below 2 data copies) until capacity exists. | read: `placement-repair.md`, "With no free node" | Owner's decision. |
| F6 | no | gap | The Outcome says "a new root voter installs the root's records". It is tested only as log catch-up, and install waits on group-heal's binding (journal "Blocked on"). A3's text does not list it. | `repair_group_test.cc:417-427` | The lead decides whether this lands now or is tracked by the cluster quest. |
| F7 | no | nit | The test driver collects reports before the root read (`repair_group_test.cc:108-127`). A step that finishes during that read lets a plan built from the earlier report run. Consensus refuses it or it repeats the step, so this is harmless now. It is also the shape a production binding must avoid, because membership changes have no compare-and-swap. | read: `consensus/membership.h:121-140` | Collect reports after the busy check, and state the binding's freshness obligation in `placement-repair.md`. |

## Not run

TSan and ASan locally (CI's; no concurrency suspicion in placement code, which is single-threaded
planning plus group-owned apply). The When-done measurements (10^6 records, install time). The
root mutants and the scan mutant (reused from their jobs). The live group test at more than 1 seed
under my mutants. Consensus internals beyond `validate_change` and the membership handler. Group code
(a1e786f4 and earlier), which is out of scope.
