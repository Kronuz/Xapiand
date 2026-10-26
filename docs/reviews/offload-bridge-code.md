# Code review: Kronuz/reactor `feat/on-reactor-marker` and Xapiand `fix/offload-bridge` (offload-bridge C1 to C6)

- Date: 2026-10-26. Concurrency tier. Reviewer: code-reviewer (fresh context).
- Reactor: `d381e4d` against origin/main `ab4b5e2`, worktree `/Users/kronuz/code/Kronuz/reactor-wt/on-reactor-marker` (read only).
- Xapiand: `fix/offload-bridge` `c5c004740` against master `2726fd225` (10 commits, 19 files).
- Checked against: `docs/reviews/engine-offload-architect.md` (C1 to C6), `docs/status/offload-bridge.md`, engine-core CR1/CR2, pool CR5.
- No GPL file was opened (`src/xapian/`, `src/sortable_serialise.{cc,h}`, `src/server/remote_protocol_views.cc`).

## Verdict

**Reactor `d381e4d`: APPROVE TO LAND** (strictly additive; one non-blocking test gap, CR4).

**Xapiand `fix/offload-bridge`: APPROVE WITH CONDITIONS.** C2 to C6 do what the ruling says on the paths exercised; the
guard fires, the condition-by-condition revert checks are credible, and I found no new deadlock, no regression against the
pool-fix binary, and no ASan or guard hit in the failover run. One condition (CR1): database teardown on the replication and
remote connection loops is not covered by C2 to C5 or by the guard, and the ledger's "database work leaves every reactor loop"
claim has to say so (or the two sites move to the pool). No blocker for landing with the pool off.

Blocking items: none. Conditions: CR1 (ledger wording, or a fix, plus a follow-up); CR5 (landing order and repin).

## Findings

### CR1. Condition. Database teardown still runs on the replication and remote connection loops, and the guard cannot see it

`serve_replication_connection` and `connect_and_replicate` (`src/server/replication_protocol_service.h:~175-185, ~235-250`) and
`serve_remote_connection` (`src/server/remote_protocol_service.h:~152-195`) keep the protocol object as a coroutine local
(`ReplicationProtocolViews client`, `RemoteProtocolViews client`). Each message is handled on the pool by `co_spawn`, but the coroutine
resumes on the loop, so the object is destroyed on the loop at connection end (EOF, hangup, exception, `closing()`), the same
shape as EO2.3 (the HTTP `Request`) that C4 fixed for the restore indexer.

What runs there for replication (`src/server/replication_protocol_views.cc:234-262, :296-319`, readable): `reset()` calls
`switch_shard->close()` (`Shard::do_close`, `src/database/shard.cc:768-811`, which commits when modified and not transactional and
always destroys the Xapian database object with `engine_shard.reset()`), then `database_pool->checkin(switch_shard)`
(`pool.cc:999`, `ShardEndpoint::checkin`, which for a writable shard may run an in-place autocommit when the committer refuses,
and `wal_writer->delete_wal` for a restore); then `lk_shard_ptr.reset()` (another checkin) and, when `temp_directory` is non-empty,
`delete_files(temp_directory)` (recursive file removal). All of it on the loop thread. I could not read the remote dtor (GPL file),
so I cannot say what `~RemoteProtocolViews` does; it holds a `lock_shard` (`remote_protocol_views.h:63`).

Why the guard is blind: it sits at `engine::sync_wait`, `DatabasePool::checkout` and `DatabasePool::cleanup`. `checkin`, `Shard::close`,
`engine_shard.reset()` (a Xapian destructor, which can itself flush), and `delete_files` pass none of them. They hit it only when
the dtor happens to reach a `Shard::commit` (the in-place autocommit fallback, or `do_close` on a non-transactional modified
shard). The contributor's cluster runs (`cluster_check.sh 2/3`, and my `FAILOVER=1 ... 3` run: 0 guard hits, 0 ASan) end connections
cleanly with nothing open, so they do not reach it either.

Not demonstrated at runtime (an abnormal replication teardown, a drop mid-transfer, is needed; I did not build one). The claim
is by reading, with the coroutine-resumes-on-the-loop step being asio's `use_awaitable` rule.

What I ask: either (a) move the teardown to the pool: destroy the protocol object in a job posted to `reactor->pool()` (heap-hold it
and `asio::post` the release), or call `client.reset()` on the pool at the end of the coroutine; or (b) record in
`docs/status/offload-bridge.md` and in the C1 text for D13 that replication and remote connection teardown remain loop-side, with
a follow-up, and add `engine::check_may_block` to `DatabasePool::checkin` so the next violation is loud. The architect should choose;
(a) is small but touches protocol code whose remote half I cannot read.

### CR2. Note. The ledger's "C5 done" is right for what the guard saw; two behaviours of the new DB_UPDATED dispatch deserve a line

`Discovery::on_message` now dispatches `DB_UPDATED` to the control worker (`discovery.cc:291`), queued FIFO with
`RAFT_APPLY_COMMAND`, `RAFT_SET_LEADER_NODE`, `ELECT_PRIMARY*` and `PRIMARY_UPDATED`.

Deadlock question (can an election wait on a DB_UPDATED that waits behind it): I found no such edge. Every job on that worker
is run to completion without waiting on another queued job or on the loop: `_ASYNC_elect_primary` uses `lock(0)` (non-blocking,
`discovery.cc:696-697`), results leave through `message_send_args.enqueue` plus a signal; `db_updated` only resolves settings
(cached after the first read; `index_resolver_lru.cc` has no waits, futures or sleeps) and arms a debounced replication trigger;
`ELECT_PRIMARY` from `resolve_index_settings` (`index_resolver_lru.cc:224`) only enqueues and signals. The loop never waits on the
worker (`control_offload` posts, `then` is posted back). Cross-node: the remote and replication services serve on their own
reactor pools, not on the peer's control worker. So the shape is latency coupling, not a cycle.

Two real behaviour changes, both small:

1. `Node::touch_node(remote_node, false)` in `db_updated` (`discovery.cc:~503`) was a liveness refresh on the bus loop at message
   time; it now happens when the worker reaches it. A long `setup_node` (up to 11 x 100 ms plus remote opens) or `load_nodes` ahead of
   it delays `last_seen`, which `index_resolver_lru.cc:~219` compares with `stalled` to decide whether to start an election. Before
   READY the message is ignored anyway, so the exposure is a busy worker while READY.
2. `dispatch_command_async_cb` drains with `while (try_dequeue)` inside one job (`manager.cc:1573-1577`); under a sustained DB_UPDATED
   rate at least as fast as the worker, a queued shutdown readiness job (`manager.cc:641`) never starts. Needs a write rate
   far above anything the tests produce; no change asked, worth a bounded drain if it ever shows.

No action required for landing.

### CR3. Verified, not a finding: `restore_stress` 400 versus 204 is the probe's race

Ledger claim holds. Same script, same host, same ASan build flags: the pool-fix binary
(`/Users/kronuz/code/Kronuz/Xapiand-wt/pool-concurrency/.scratch/fix-bin/xapiand`) and this branch both now return mostly 400 and a few
204 for the concurrent PUTs (`.scratch/review/restore_ab.out`: fix-bin 17x400 + 1x204 and 16x400 + 2x204; branch 16x400 + 2x204 and 18x400).
Which request types `_id` first is the race between the RESTORE body and the first PUT; no code path in this branch changes that
ordering (the indexer is created at `search_views.cc:1684/1703` as before).

### CR4. Non-blocking (reactor). The `UdpServer` change has no test

`reactor_udp.h` now runs its loop through `reactor_.run()`. Reverting that line alone (`.scratch/review/rmut_c`, `udp_test` with the
change undone) still prints `PASS (0 failures)`; the new `[M]` checks cover `Reactor::run()` and `TcpServer` only (mutants: scope removed
from `Reactor::run()` fails 2 of 8, `TcpServer` bypassing `run()` fails 1). My probe `.scratch/review/reactor_probe.cc` shows the UDP
handler is marked and the state is clean after `stop()`, so the code is right; add one check to `udp_test` (handler calls
`reactor::inside_run()`). Xapiand exercises it at runtime (the guard caught `db_updated` on the bus loop).

### CR5. Condition (landing order). Xapiand pins a feature-branch SHA

`CMakeLists.txt:~792` pins reactor to `d381e4d` (on `feat/on-reactor-marker`). Land reactor first; if it lands by anything other than a
fast-forward (squash, rebase) `d381e4d` is not on `main`, so repin Xapiand to the landed SHA before merging into master, and drop the
"lead repins" comment. The built tree used `d381e4d` (`.scratch/build-asan/_deps/reactor-src`), declared before `http_lib` so it wins over
the `e19afd9` pins in http and cluster; only one reactor copy is in `_deps`.

### CR6. Note. Binary version string

The ASan binary reports `git:d49a1d0b8` (the C6 commit), not `0191530a2`; the version header is fixed at configure time, so it does not
tell which sources were built. Binary mtime (08:16:03) is after the last source edit (`manager.cc` 08:15:32), and the cluster runs show the
DB_UPDATED dispatch, so I treated it as current. A re-run for the final handoff should reconfigure.

## Reactor review (`d381e4d`), by the brief

- Additive: `inside_run()`, `run_depth()`, `RunScope`, `Reactor::run()` are new names; `TcpServer` and `UdpServer` call `Reactor::run()`
  where they called `io().run()` (same return and exception behaviour, plus a thread-local counter). No existing signature changed or
  removed. Name clash risk for a subclass defining `run()` on `Reactor`: none in this repo and none in Xapiand; Detent's sources were not
  searched.
- Counter nests (`RunScope` increments), unwinds on exceptions out of `run()` (probe: depth 0 after a throwing handler escapes
  `run()`, and `run()` after `restart()` is marked again), and is clear on the same thread after `run()` returns and on pool threads.
  Re-entrant `poll()` and `run()` inside a handler stay marked (probe). A bare `io().poll()` or `io().run()` on a host thread is not
  marked, as the API note says; `TcpServer::start`'s error path polls unstarted contexts (`reactor.h:~425`), which is that gap and
  only drains startup work.
- `static thread_local` inside an `inline` function in a header-only library: one counter per image. Fine for static links (Xapiand);
  a consumer that loads reactor into separate shared objects would get one per object. No such consumer today.
- Build and tests: Debug with ASan+UBSan (`-fno-sanitize-recover=undefined`), Apple clang, 8 of 8 ctest pass (`rbuild/ctest.log`).

## Xapiand review, by condition

- C2 one bridge: `git grep -n 'task::sync_wait' src` lists `src/engine/sync.h` only (code at `:46`; the other hit is the include comment).
  Both `checkout` overloads and `cleanup` call the guard (`pool.cc:989, 1014, 1096`). `NDEBUG` is undefined exactly when `XAPIAND_ASSERTS`
  is on (`CMakeLists.txt:117-124`), so the abort and the count-and-log-once branches match the ruling. `check_may_block` reads
  `task::current()` on the caller's thread, as the ruling asks.
- C2 test soundness: `guard_fires` counts any SIGABRT, so a body that aborts for another reason would pass. The same bodies are the
  controls in `test_quiet_off_loop` (plain thread, pool thread, loop thread after `run()`), which assert no abort, so the fire cases
  are the guard. The scheduler-thread case uses a never-posted scheduler to set `task::current()`; control `wait_answer` is quiet.
- C3: `should_offload` no longer lists FLUSH; both comments corrected. OPTIONS/OPEN/CLOSE/QUIT stay inline per the ruling.
- C4: `DropIndexer` is declared after the local `indexer` (`search_views.cc:1656-1671`), so `request.indexer.exchange(nullptr)` plus
  `finish()` run first and the local's last reference then drops, both on the worker, on every exit including exceptions. `request.indexer` is
  set only in the restore view (grep), so no other view leaves a last reference to `~Request`. The response reads copies (`prepared()`,
  `results()`), so dropping before the response is written is safe. `~Request` still calls `finish()` on whatever it holds (null now).
- C5: `reactor(1, 0)` gives the control plane one worker; `control_offload` posts to the pool (bypassing the gate, intended), carries the
  exception back and rethrows it on the loop inside `run()`, as the handler did before. `_new_cluster` and `node_name` are written on the worker and read in `then`
  after the post back (ordered by the post). The `Setup` struct keeps `local_node` re-assigned on the worker for the `then` side, as the
  original did. `join()` stops and joins the worker after the loop has stopped and before the pool finishes (`manager.cc:1222-1226`);
  `break_loop` is `io_context::stop()`, thread-safe, so the worker may call it. The pool is created in the manager constructor, after the
  detach fork (`main.cc:1001` before `server()`), so no thread is lost across `fork`.
  `shutdown_impl` keeps `now != 0` as an immediate break, and makes the graceful check asynchronous; the 5 s timer re-arms as before.
- C6: `Offload`, `set_offload`, `XAPIAND_ENGINE_OFFLOAD_THREADS` are gone; `Hop` takes a `scheduler_ref` (empty means inline). Production
  code never passes one, so behaviour is the pool-off path.
- Guard hits found by the contributor (shutdown `is_pending`, `db_updated` on the bus loop) are real and the fixes are the right shape.

## Probes and runs

Build reuse: the contributor's `.scratch/build-asan` (Debug, ASan, asserts on; current as above). No Xapiand rebuild.

1. `FAILOVER=1 harness/cluster_check.sh 3` with the guard armed (3 nodes, leader killed): membership, write/read, search, leader
   election and rejoin pass; 0 guard hits and 0 ASan reports in the three node logs (`xc-failover/`, `failover.out`). The two failover
   checks "a write through node2 succeeds with node1 down" and "reads back from node3" FAIL (503 after 120 s, "Endpoint node is inactive").
   The pool-fix binary fails the same two checks the same way (`xc-failover-base/`, `failover-base.out`), so it is pre-existing and not
   this branch (a shard primary is not re-elected after its node dies); worth its own ticket.
2. `restore_stress.sh` A/B against the pool-fix binary, four runs each (`restore_ab.out`): same 400/204 mix (CR3).
3. Reactor: `reactor_probe.cc` (UDP handler marked, exception out of `run()`, nested poll/run, thread reuse), plus mutants
   `rmut_a` (no scope in `Reactor::run()`: 2 of the new checks fail), `rmut_b` (`TcpServer` bypasses `run()`: 1 fails), `rmut_c` (`UdpServer`
   bypasses `run()`: all tests pass, CR4).
4. Read: the callers of every path I could reach from a loop: control-plane callbacks, `Discovery::on_message` (all message types), the
   replication and remote connection coroutines, `Request` teardown, `ShardEndpoint::checkin`, `Shard::autocommit`, `Shard::do_close`.
   `set_cluster_database_ready_async_cb` and the signal path do no database work.

Files kept in `/Users/kronuz/code/Kronuz/Xapiand-wt/offload-bridge/.scratch/review/`: `reactor_probe.cc`, `reactor_probe`, `reactor_probe.out`,
`rbuild/` (reactor ASan build, `ctest.log`), `rmut/`, `rmut_a/`, `rmut_b/`, `rmut_c/`, `failover.out`, `xc-failover/`, `failover-base.out`,
`xc-failover-base/`, `restore_ab.out`.
Note: the probe prints for the nested `run()` case show `nested=0`: that is asio (an inner `run()` that runs out of work stops the context so
the outer never reaches the later handler), not the marker.

## Not run

Linux and FreeBSD; TSan (the contributor ran it on the pool tests and `test_blocking_guard`; the new `control_offload` path is not under TSan);
a Release build of the guard's count-and-log branch; a mid-transfer replication drop (CR1's runtime proof); the docs E2E, `hammer.sh`,
`uafhunt.sh`, `dump_restore_loop.sh` and a Xapiand rebuild (the contributor's evidence taken as given for the binary I checked for currency);
Detent's use of reactor; any timing; the GPL files; the remote protocol destructor (`~RemoteProtocolViews`).

## Re-check (2026-10-26)

Scope: CR1, CR4, CR6 on `fix/offload-bridge` (code head `27a9dc463`, branch head `89de84a29` differs only in `docs/status/offload-bridge.md`) and
Kronuz/reactor `test/udp-inside-run` (`0c04d0d`). Probes and outputs in `.scratch/review/recheck/` (Xapiand worktree) and
`.scratch/recheck/` (reactor worktree). No tracked file was edited; `git status` is clean in the Xapiand worktree.
Reverts were done out of tree: `recheck/mkvariant.sh` compiles `manager.cc`, `replication_protocol_service.cc` (and `pool.cc`) against copies of the
`c5c004740` text and relinks `xapiand-rev` / `xapiand-pre` from the build-asan objects (a first attempt to overwrite the header in place was refused).

### CR1: PARTIAL

Closed for the normal-end and error exits, open for the cancellation exit the brief names.

Closed parts:
- Guards: `src/database/pool.cc:1003` (`checkin(shared_ptr<Shard>&)`) and `:1045` (`checkin(vector<...>&)`) call `engine::check_may_block("DatabasePool::checkin")`.
  `xapiand_test_blocking_guard` (existing ASan binary) prints `OK: 12 checks passed`.
- Move: `replication_protocol_service.h:223` (serve) and `:266` (connect_and_replicate) hand the `unique_ptr` to `engine::release_on_pool` after the
  try/catch, so normal end, EOF, hangup and error all reach it. Lifetime across the hop: `release.h:48-53` runs `object.reset()` in a job on
  `reactor->pool()` while the posting coroutine is suspended holding `object`; nothing else holds the views (`SocketAbortable` keeps only the fd; the
  `~ReplicationProtocolViews` at `replication_protocol_views.cc:234` does not touch the fd; in `connect_and_replicate` the socket is destroyed before
  the release, as before). `socket.shutdown` now runs before the views are destroyed instead of after; harmless.
- Revert check, `recheck/repl_drop.sh` (the contributor's script, scratch path moved), 3 rounds each, same host and ports:
  - `xapiand-fix` (current binary): `guard hits node2=0 node1=0; ASan=0` in all rounds (replication connection seen and node1 killed in 2 of 3).
  - `xapiand-rev` (service header at `c5c004740`, `pool.cc` guard kept): `guard hits node2=6` in all 3 rounds, abort stack
    `check_may_block` <- `DatabasePool::checkin(shared_ptr<Shard>&)` <- `lock_shard::unlock` <- `~ReplicationProtocolViews` (`rd-rev-1/node2.log`).
    Note the reverted binary aborts even on rounds where the netstat probe missed the connection, so a normal end triggers it too.
- Remote residual: recorded in `docs/status/offload-bridge.md:27-30` (D13 text) and `:37-40` (ledger). I did not read the remote file.

Open part (cancellation): `release_on_pool` does not run its pool hop when the session coroutine has been cancelled. Asio's
`await_transform` (`asio/impl/awaitable.hpp:174-179`) throws `operation_aborted` at the `co_await engine::release_on_pool(...)` itself when the
coroutine's cancellation state is set and `throw_if_cancelled` is true (the default). The unstarted `release_on_pool` frame is then destroyed
during unwinding with the `unique_ptr` parameter still owning the views, so the destructor runs inline on the loop, and the surrounding
`catch (const std::exception&)` (`:223`) swallows the exception. Inbound sessions get a cancellation slot from `reactor::launch_session`
(`reactor.h:217`) and `TcpServer::stop()` -> `abort_all()` -> `SessionControl::abort()` emits `cancellation_type::all` into it (`reactor.h:199`), so
this is every inbound replication connection that is still open when the server stops. The outbound `connect_and_replicate` is spawned with
`asio::detached` (`replication_protocol_service.cc:223`), has no slot, and is not affected.
Probe `recheck/release_cancel_probe.cc` (real `reactor::TcpServer`, real `engine/release.h`, a session shaped like `serve_replication_connection`,
a stand-in whose destructor records `reactor::inside_run()`): normal EOF -> destroyed on the pool; `server.stop()` with the connection open ->
destroyed on the loop (`dtor ran on a loop thread: YES`). With `co_await asio::this_coro::throw_if_cancelled(false);` placed before the
`release_on_pool` call (`release_cancel_probe_fixed.cc`) both cases destroy on the pool. The call has to be at the call site: setting it inside
`release_on_pool` is too late because the throw is in the caller's `await_transform`.
Runtime on the real binary not shown: `recheck/repl_term.sh` (SIGTERM the primary mid-replication, fix binary, 3 rounds) saw the connection each time
but node1 was still in "Waiting for replicators" / "Remote Protocol is busy" at the 20 s mark, never reaching the session cancellation; 0 guard hits
and 0 ASan. Whether the serving side's views hold a shard to check in (so that the guard would fire) was not established; the loop-side
destruction itself is shown by the probe. Ask: call `throw_if_cancelled(false)` before the release in `serve_replication_connection` (and in
`connect_and_replicate` for symmetry), or say in the ledger that the hop is skipped on cancellation; add a case to the blocking-guard test or a
reactor-level check for it.

### CR4: CLOSED

`test/udp_test.cc` (`0c04d0d`): the unicast handler stores `reactor::inside_run()` and `check(marked.load(), "[M] the handler runs inside_run()")`
follows the `got >= 1` wait (the store precedes `got.fetch_add`, so the read is ordered). ASan+UBSan, Xapiand's Asio, `.scratch/recheck/`:
`udp_good` (as is) `PASS (0 failures)`; `udp_mut` (`reactor_udp.h` line 147 changed to `reactor_.io().run()` in a copy of the headers)
`FAIL (1 failures)`: `FAIL: [M] the handler runs inside_run()`.

### CR6: CLOSED

`strings .scratch/build-asan/bin/xapiand`: `Xapiand v1.0.0-alpha.9 (rev:20261026172707) (git:27a9dc463)`. `27a9dc463` is the last commit that touches
`src/` (`git diff --stat 27a9dc463 HEAD` is `docs/status/offload-bridge.md` only), the binary (10:32:28) is newer than the sources it contains
(10:31:40), and it carries the new guard string (`DatabasePool::checkin`; the pre-CR1 build has none). `.scratch/xapiand-head` is byte-identical to it.

### uafhunt caveat: confirmed

`recheck/uafhunt.sh` (the contributor's script, 10 rounds requested, default ASAN_OPTIONS so container-overflow is on), one run each, in parallel
on different ports: pre-CR1 build (`xapiand-pre`: service header and `pool.cc` at `c5c004740`) and the fix build both stop at `rounds=8` with
`ERROR: AddressSanitizer: container-overflow` and the same stack tail `prometheus::detail::CKMSQuantiles::insertBatch()` (`ckms_quantiles.cc:122`)
<- `prometheus::Summary::Observe` <- `emit_via_writer`. It is not this branch's. Cause not chased.

### New findings

None from the fix. One note outside scope: `recheck/repl_term.sh` shows a primary with an active replication connection takes more than 20 s to
shut down on SIGTERM ("Waiting for replicators"); not examined.

### Not run

Linux and FreeBSD; TSan; the docs E2E, `cluster_check.sh`, `hammer.sh` (the ledger's runs taken as given); the mutants on `test_blocking_guard`
(only the unmutated 12-check run); a Release build; the remote protocol and its destructor; a real-binary proof of the cancellation path
(a graceful stop reaching an open inbound replication session); the reactor's full ctest (only `udp_test`).
