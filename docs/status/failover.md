# failover
- Branch / worktree: fix/failover at /Users/kronuz/code/Kronuz/Xapiand-wt/failover
- State: fixing (the election fix is verified; the bus-handler guard is committed but not yet built; host under a strict hold until about 01:17)
- Done:
  - 188e2d6ab cherry-pick of the FAILOVER harness from master (15596cf72)
  - 1c3013b94, 2a999f1f4 ledger and recorded root cause
  - 8a5bd916a Promote a replica when a shard's primary dies (src/server/primary_election.h, src/server/discovery.cc, src/index_resolver_lru.cc, tests/test_primary_election.cc)
  - (this commit) Guard the discovery bus handler so an exception cannot end its receive loop (src/server/discovery.cc), not built yet
- Doing: waiting for the strict host hold to lift (`quiet check` exits 1: strata-trials until 01:17:22)
- Next, once `quiet check` exits 0 (or 2: nice -n 19, at most 4 jobs):
  1. `cmake --build build` (the guard commit has never been compiled), then `(cd build && ctest --output-on-failure)`.
  2. `FAILOVER=1 BIN=build/bin/xapiand harness/cluster_check.sh 3`.
  3. The fast-kill variant `.scratch/failover_fast_kill.sh 3` (cluster_check with sections 2 and 3 cut, so the leader dies right after the first write) with `FAILOVER=1`: it must now elect a leader; if not, trace the bus again.
  4. `harness/cluster_check.sh 2` and `harness/cluster_check.sh 3`.
  5. `PATH=~/.scratch/newman6/node_modules/.bin:$PATH harness/run_all.sh smoke load e2e`.
  6. Ledger to "ready to land".
- Blocked on: the host hold (strata gate VM trials), nothing else. Owner decisions listed under Findings.
- Findings:
  - Symptom: survivors log `PUT /cluster_probe/2: DatabaseNotAvailableError: Endpoint node is inactive` (src/database/shard.cc:329) every second for 120 s; no shard is ever promoted. A write to an index needs every shard's primary, so one shard with a dead primary (cluster_probe/.__3, primary node1, only replica node2: 5 shards, 1 replica, 2 copies each) 503s the whole index.
  - The 503 path: resolve_index_endpoints (src/index_resolver_lru.cc) finds the shard's first node inactive and rebuilds; update_primary waits --database-stall-time, then dispatches ELECT_PRIMARY every 5 s; the write still goes to the dead primary and DatabaseNotAvailableError maps to 503 (src/server/http_utils.h:66).
  - Cause 1 (deterministic): the leader's tally in Discovery::_ASYNC_elect_primary_response required `Node::quorum(total_nodes, voters.size())` and `Node::quorum(total_nodes, ok_nodes)` over all copies, the dead primary included. With 1 replica that is 2 of 2, unreachable. Instrumented: leader logs `voters cluster_probe/.__3 = 1 of 2` every round.
  - Cause 2: the leader ignored a repeat vote (`if (emplaced.second)`) and kept voters 600 s, so later rounds never re-tallied.
  - Cause 3: a member that had not pulled its replica yet (pulls run 0-3 s after DB_UPDATED) never answered (`catch (...) { }`); run3 logged `No glass database found at path 'cluster_probe/.__3'` on node2.
  - Fix (8a5bd916a): the Raft leader already holds the cluster majority, so primary_election::elect waits for every live copy to vote and promotes the best (existing copy over missing, then highest revision, then shard order); a repeat vote replaces the old one; a member without its copy votes as an empty copy; update_primary promotes the elected copy, not just the first live one. Promoting an empty or behind copy loses writes only the dead primary held, the documented acknowledged-but-lost window (ARCHITECTURE.md), now possibly the whole shard when its replica never pulled. Owner may prefer to keep such a shard unavailable instead.
  - Second defect, pre-existing (same result on the pre-fix binary .scratch/xapiand-prefix): kill the leader within about a second of the first write and the survivors never elect a leader, even at 120 s. Trace: both survivors keep sending RAFT_REQUEST_VOTE (terms 2 to 17) but receive nothing after the kill, not even their own looped-back multicast. Cause: reactor::UdpServer::receive_loop (Kronuz/reactor, reactor_udp.h) runs the handler inside its loop's try; any exception ends the loop for good. Discovery::on_message runs db_updated on the bus thread, which calls resolve_index_settings(..., writable) and does remote I/O to the dead leader; the NetworkError escapes (only DatabaseNotAvailableError and MissingTypeError are caught) and the node goes deaf. Xapiand-side fix in this commit: catch and log around on_message. Library-side (not this repo): the reactor receive loop should survive a handler exception, and db_updated should not do blocking remote I/O on the bus thread (AGENTS.md: never block a reactor thread). Both for the lead to route.
  - Election timing (14-16 s): configured, not a defect. src/server/discovery.cc sets heartbeat 1 s and an election timeout uniform in [10 s, 30 s] (10 s = NODE_LIFESPAN, src/node.h:47); the first of two followers fires at 10 + 20/3 = 16.7 s on average, observed 10-25 s. Shortening it is an owner decision, coupled to NODE_LIFESPAN liveness.
  - Also seen, not fixed: after rejoin, the restarted node lacks replicas it never pulled (cluster_probe/.__1, .__4 on node1) until the next DB_UPDATED for them.
- Last verified (2026-10-17):
  - Before the fix: FAILOVER=1 cluster_check.sh 3 RED in 4 runs (leader in 11-25 s, PUT 503 at 120 s).
  - 8a5bd916a: ctest 8/8 pass (new test_primary_election included); FAILOVER=1 cluster_check.sh 3 GREEN in 3 of 3 runs (new leader in 18, 13, 10 s; write through node2 at 24, 19, 17 s after the kill; reads back from node3; rejoin keeps one leader).
  - Fast-kill variant: RED on both the pre-fix and fixed binaries (the second defect).
  - Not yet run: the guard commit at all; cluster_check.sh 2 and 3 without FAILOVER; run_all.sh smoke load e2e.
