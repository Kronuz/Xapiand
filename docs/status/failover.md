# failover
- Branch / worktree: fix/failover at /Users/kronuz/code/Kronuz/Xapiand-wt/failover
- State: ready for review (code review, and the security review of the vote path per SYS-14); final ASan+UBSan cluster runs in progress
- Done:
  - 188e2d6ab cherry-pick of the FAILOVER harness from master (15596cf72)
  - 1c3013b94, 2a999f1f4 ledger and recorded root cause
  - 8a5bd916a Promote a replica when a shard's primary dies (src/server/primary_election.h, src/server/discovery.cc, src/index_resolver_lru.cc, tests/test_primary_election.cc)
  - c51da3e09 Guard the discovery bus handler so an exception cannot end its receive loop (the stopgap; removed in 4f378a51c)
  - 7af210738 Restore the database-UUID check (SYS-14): the shard's database is the UUID most live copies hold, a tie to shard order; a copy of another database is never promoted, revisions compare only within the shard's database; an empty copy stays promotable. 5 new tests (22 checks); mutants without the filter (6 fail) or counting dead copies (1 fails) are caught.
  - 971419646 ledger: the failover matrix at 7af210738
  - dbbbec642 Pin reactor 1c37def and wire its L_EXC seam (reactor review C1): src/xapiand_reactor_trace.h maps reactor's L_EXC onto Xapiand's real async L_EXC, set target-wide with REACTOR_TRACE_HEADER; the reactor declaration moves ahead of http's, whose CMake pins reactor e19afd9 and would otherwise win (first FetchContent declaration wins), so the old position left Xapiand's own pin unused.
  - 4f378a51c Drop the bus-handler catch (C1): the fast-kill case passes without it (table below).
- Doing: final ASan+UBSan build (pointer-overflow excluded, see Findings) and the failover cases on it
- Next:
  1. Code review of the branch; the reviewer probes the old primary returning after a promotion and two copies with different UUIDs (SYS-14).
  2. Security review of the vote path before landing (SYS-14): a forged ELECT_PRIMARY_RESPONSE over unauthenticated UDP (TS-R13) can replace a live copy's vote in any round, since a repeat vote now replaces the earlier one; with the UUID check, a forged vote can also shift which database counts as the majority.
  3. c51da3e09's catch is gone (4f378a51c) now that Xapiand pins the fixed reactor; nothing else of the stopgap remains.
  4. For the lead to route, not this branch: db_updated still does blocking remote I/O on the bus thread (resolve_index_settings to the dead leader), which AGENTS.md forbids on a reactor thread; it is why DB_UPDATED throws in the fast-kill case. RapidJSON v1.1.0's UBSan report (Findings).
- Blocked on: nothing. Owner decisions are recorded under "Owner decision".
- Findings:
  - Symptom: survivors log `PUT /cluster_probe/2: DatabaseNotAvailableError: Endpoint node is inactive` (src/database/shard.cc:329) every second for 120 s; no shard is ever promoted. A write to an index needs every shard's primary, so one shard with a dead primary (cluster_probe/.__3, primary node1, only replica node2: 5 shards, 1 replica, 2 copies each) 503s the whole index.
  - The 503 path: resolve_index_endpoints (src/index_resolver_lru.cc) finds the shard's first node inactive and rebuilds; update_primary waits --database-stall-time, then dispatches ELECT_PRIMARY every 5 s; the write still goes to the dead primary and DatabaseNotAvailableError maps to 503 (src/server/http_utils.h:66).
  - Cause 1 (deterministic): the leader's tally in Discovery::_ASYNC_elect_primary_response required `Node::quorum(total_nodes, voters.size())` and `Node::quorum(total_nodes, ok_nodes)` over all copies, the dead primary included. With 1 replica that is 2 of 2, unreachable. Instrumented: leader logs `voters cluster_probe/.__3 = 1 of 2` every round.
  - Cause 2: the leader ignored a repeat vote (`if (emplaced.second)`) and kept voters 600 s, so later rounds never re-tallied.
  - Cause 3: a member that had not pulled its replica yet (pulls run 0-3 s after DB_UPDATED) never answered (`catch (...) { }`); run3 logged `No glass database found at path 'cluster_probe/.__3'` on node2.
  - Fix (8a5bd916a): the Raft leader already holds the cluster majority, so primary_election::elect waits for every live copy to vote and promotes the best (existing copy over missing, then highest revision, then shard order); a repeat vote replaces the old one; a member without its copy votes as an empty copy; update_primary promotes the elected copy, not just the first live one. Promoting an empty or behind copy loses writes only the dead primary held, the documented acknowledged-but-lost window (ARCHITECTURE.md), now possibly the whole shard when its replica never pulled. Owner may prefer to keep such a shard unavailable instead.
  - Second defect, pre-existing (same result on the pre-fix binary .scratch/xapiand-prefix), fixed by reactor 1c37def (dbbbec642): kill the leader within about a second of the first write and the survivors never elect a leader, even at 120 s. Trace: both survivors keep sending RAFT_REQUEST_VOTE (terms 2 to 17) but receive nothing after the kill, not even their own looped-back multicast. Cause: reactor::UdpServer::receive_loop (Kronuz/reactor, reactor_udp.h) runs the handler inside its loop's try; any exception ends the loop for good. Discovery::on_message runs db_updated on the bus thread, which calls resolve_index_settings(..., writable) and does remote I/O to the dead leader; the NetworkError escapes (only DatabaseNotAvailableError and MissingTypeError are caught) and the node goes deaf. Xapiand-side fix in this commit: catch and log around on_message. Library-side (not this repo): the reactor receive loop should survive a handler exception, and db_updated should not do blocking remote I/O on the bus thread (AGENTS.md: never block a reactor thread). Both for the lead to route.
  - Election timing (14-16 s): configured, not a defect. src/server/discovery.cc sets heartbeat 1 s and an election timeout uniform in [10 s, 30 s] (10 s = NODE_LIFESPAN, src/node.h:47); the first of two followers fires at 10 + 20/3 = 16.7 s on average, observed 10-25 s. Shortening it is an owner decision, coupled to NODE_LIFESPAN liveness.
  - UUID check, a reviewable choice: no record of a shard's database UUID exists (IndexSettings and the Raft state carry none), so the reference comes from the votes; the old code compared against whichever voter answered last. A tie between databases goes to shard order; refusing to elect on a tie is the alternative, at the cost of availability, which SYS-14 ranks first.
  - Reactor seam: Xapiand's L_EXC is async and keeps the std::current_exception() pointer, which owns the exception object, so reactor's contract item 4 (no later reads of datagram views) holds unless an exception type stores a view into the datagram; Xapiand's own exceptions own their strings. Without the app catch the log line reads `ERROR: UDP datagram handler died with an unhandled exception: NetworkError: connect failed (...)`, losing only the message-type name.
  - UBSan, pre-existing and not this branch's: RapidJSON v1.1.0 (CMakeLists.txt:460, :993; unchanged on this branch) applies an offset to a null pointer in internal/stack.h:117 on the first JSON write of an error response (SearchService::on_error -> write_status_response -> MsgPack::to_string). A solo ASan+UBSan node aborts on one GET to a missing index. CI's sanitizer job runs ASan and TSan, not UBSan, so CI never sees it. The build uses -fno-sanitize-recover=all, so a runtime suppression cannot skip it, and Xapiand's CMakeLists overwrites the per-config flags while the sanitizer module appends -fsanitize=undefined after CMAKE_CXX_FLAGS, so a -fno-sanitize flag there is undone; the final local ASan tree passes `-fsanitize-ignorelist=.scratch/ubsan-ignore.txt` (`[pointer-overflow]` / `fun:*rapidjson*`), which skips that one check in RapidJSON's functions only (verified on a ten-line RapidJSON repro: reported without it, clean with it).
  - Apple clang on x86_64 runs ASan here (the arm64 hang in memory/kronuz-cpp-build-tooling.md does not apply); Homebrew LLVM 23 does not build Xapiand's pins (RapidJSON v1.1.0's GenericStringRef::operator=, and XXH_STATIC_LINKING_ONLY redefined under -Werror), and needs CMAKE_OSX_SYSROOT set or it puts the SDK's usr/include ahead of libc++. LeakSanitizer is unsupported on macOS x86_64 (drop detect_leaks).
  - Also seen, not fixed: after rejoin, the restarted node lacks replicas it never pulled (cluster_probe/.__1, .__4 on node1) until the next DB_UPDATED for them.
- Last verified (2026-10-17):
  - 4f378a51c, Release (Apple clang 17), nice 19; .scratch/final.sh, logs in .scratch/final/ (summary.txt). ctest 8/8.

    | Case | Pass | New leader (s) | Write resumes (s after kill) |
    |---|---|---|---|
    | `FAILOVER=1 harness/cluster_check.sh 3` | 3/3 | 14, 28, 24 | 21, 43, 30 |
    | `FAILOVER=1 .scratch/failover_fast_kill.sh 3` | 3/3 | 12, 16, 12 | 19, 23, 18 |
    | `harness/cluster_check.sh 2` | 1/1 | | |
    | `harness/cluster_check.sh 3` | 1/1 | | |
    | `harness/run_all.sh smoke load e2e` | 1/1 (3 layers GREEN) | | |

    The 43 s write resume is the slowest of the 26 Release failover and fast-kill runs on this branch (it follows a 28 s election); the check allows 120 s.
  - C1 A/B at dbbbec642 (reactor 1c37def), fast-kill case `FAILOVER=1 .scratch/failover_fast_kill.sh 3`, interleaved, Release, nice 19; .scratch/c1.sh, logs in .scratch/c1/ (summary.txt). "catch" is dbbbec642; "nocatch" is dbbbec642 with c51da3e09's catch removed, the source of 4f378a51c:

    | Variant | Pass | New leader (s) | Write resumes (s after kill) | App catch / reactor catch per run |
    |---|---|---|---|---|
    | catch | 5/5 | 12, 12, 13, 29, 13 | 18, 18, 19, 35, 19 | 4 / 0 |
    | nocatch | 5/5 | 21, 20, 12, 21, 16 | 27, 27, 18, 27, 23 | 0 / 4 |

    Same four DB_UPDATED NetworkErrors per run either way; with no catch at all (pre-C1 reactor) the case was RED. The timing spread overlaps the matrix's (10-29 s to a leader) and is the configured 10-30 s election timeout, not the catch.
  - ASan+UBSan (Apple clang 17, RelWithDebInfo, LTO off) at dbbbec642: ctest 8/8 (ASAN_OPTIONS=halt_on_error=1:abort_on_error=1). The same binary on the fast-kill case: node1 aborts within about 20 s of startup, 2 of 2 runs, on the pre-existing RapidJSON report (Findings), before any kill.
  - 7af210738, Release (Apple clang 17, x86_64), every run at nice 19 under the light hold; logs and kept rundirs in .scratch/matrix/ (summary.txt):

    | Case | Command | Pass | New leader (s) | Write resumes (s after kill) |
    |---|---|---|---|---|
    | failover3 | `FAILOVER=1 harness/cluster_check.sh 3` | 5/5 | 12, 16, 10, 17, 22 | 20, 23, 17, 23, 28 |
    | fastkill3 | `FAILOVER=1 .scratch/failover_fast_kill.sh 3` | 5/5 | 21, 22, 13, 17, 26 | 28, 28, 19, 24, 33 |
    | plain2 | `harness/cluster_check.sh 2` | 3/3 | | |
    | plain3 | `harness/cluster_check.sh 3` | 3/3 | | |
    | run_all | `harness/run_all.sh smoke load e2e` | 1/1 (3 layers GREEN) | | |

  - Fast-kill was RED on the pre-fix and 8a5bd916a binaries; it is GREEN with c51da3e09. Every fast-kill run logs the guard 4 times on the survivors, each `Dropping discovery message DB_UPDATED: NetworkError: connect failed (... :9880) (Connection refused)`, the predicted cause; failover3 never logs it.
  - ctest 8/8 (test_primary_election: 22 checks).
  - Before the fix: FAILOVER=1 cluster_check.sh 3 RED in 4 runs (leader in 11-25 s, PUT 503 at 120 s).
  - 8a5bd916a: ctest 8/8 pass (new test_primary_election included); FAILOVER=1 cluster_check.sh 3 GREEN in 3 of 3 runs (new leader in 18, 13, 10 s; write through node2 at 24, 19, 17 s after the kill; reads back from node3; rejoin keeps one leader).
  - Fast-kill variant: RED on both the pre-fix and fixed binaries (the second defect).
  - Not yet run: the guard commit at all; cluster_check.sh 2 and 3 without FAILOVER; run_all.sh smoke load e2e.

## Owner decision (2026-10-17, system review SYS-14)

- Keep promoting the best live copy, even an empty one: availability wins, and the loss of writes only the dead primary held is documented.
- Restore the database-UUID check the fix dropped: a copy votes only for a shard of the same database.
- Before landing, the security reviewer reviews the vote path (a forged UDP vote must not replace a live copy's vote), and the code reviewer probes the old primary returning after a promotion.
