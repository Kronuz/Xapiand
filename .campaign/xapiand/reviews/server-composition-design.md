# server-composition review: design

Commit: cpp-libraries:2fd6ecb (`docs/server-composition.md`, unchanged at cpp-libraries:260b9df),
checked against task:95798a0, http:ec273dd, reactor:bf874dc, Detent:2c9d2bac, prism:4d3c96d and
Xapiand:eb3dcfa80
Verdict: CHANGES
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right: one control loop on the main thread, one `Limits` taken whole, one TLS builder,
`serve` and `stop_signals` from service-shape, housekeeping as wheel tasks owned by services. It
matches Xapiand D62 and D71-D77 and Detent D76 and D79-D82. Six text fixes stand between it and
adoption: one shared flag whose stated meaning no app has (F1), a descriptor budget that is placed
and sized so it contradicts itself and fails stock defaults (F2), a `Limits` that duplicates two
existing parameter sets (F3), process steps the "seven steps" leave out (F4), no rule for how code
reaches state services (F5), and one deviation left open against D82 (F6).

## Evidence I relied on

No job proves a design note. I read the note, the owner's decisions (Xapiand PLAN D62, D71-D77;
Detent PLAN D76, D79-D82), task's `docs/design/service-shape.md` (DRAFT, task:0e71a38) and every
cited file at the heads above. The note's quest (Detent `server-composition`) landed docs-only at
cpp-libraries:2fd6ecb. Its A4 is the Xapiand lead's agreement, which this review informs.

## What I checked myself

File reads only, no probe jobs: each citation in the note at today's heads, and Xapiand's
`src/manager.cc` (1,409 lines), `src/manager.h`, `src/main.cc` and `src/tls.cc` in full where they
touch the composition.

### Question 1: is the note still true at today's heads?

Nothing it proposes exists yet. task has no `serve`, `stop_signals`, `group`, `periodic` or
`debounce` (task:95798a0 adds `blocking_scheduler`, the heavy lane and the D79 `arm`/`cancel`).
http has no `Limits` and no call that closes idle connections now. reactor has no `tls::server_context`.
Its descriptions of today's code mostly hold. Line numbers in http, reactor and Xapiand opts drifted,
and three claims are wrong:

- Already there: http's constructor takes `in_flight_limit`, a `ConnectionFactory` and
  `maximum_connections`, and that bound reaches `reactor::TcpServer::set_connection_limit`
  (http_asio.h:1175-1182, reactor.h:700,712). The setters for request, idle, header and inactivity
  timeouts, `max_body` and shutdown grace exist (http_asio.h:1198-1217). Five of those values
  already sit in one struct, `AsioResponseOptions` (http_asio.h:92-107). The TLS factory takes
  handshake deadline 5 s, 64 handshakes, 1024 connections and a failure hook, and still keeps its own
  connection count (`TlsAdmission`, http_asio_tls.h:299-373). `reactor::tls::configure` refuses an
  incomplete configuration and a key that does not match its certificate (reactor_tls.h:56-79).
  Xapiand's `tls_setup` adds the key-mode check, the validity dates, `strict` and the descriptor
  check (tls.cc:51-75).
- Landed since: reactor removed `PeriodicTimer` (reactor:0d3bc71). Xapiand's shutdown retry is now a
  `control::Periodic` (Xapiand:c30d437ab, manager.cc:184,706-709), so the Mains housekeeping row
  ("`PeriodicTimer` retry") is stale. http's idle-cost work landed (http:76f698d, http:ad6771f), so
  the Sizing numbers are stale (F9). Prism's pinned `tls_connection_factory_timed` no longer exists in
  http (CHANGELOG.md:152). The relay gained a mirror service and an outbound `HttpUpstream`
  (Detent host.cc:188-207, configuration.hh:14-25).
- Drift: http constructor :1134 is now :1175, setters :1157-1176 are :1198-1217, TLS count
  http_asio_tls.h:66-70 is :299-373; reactor `set_connection_limit` :629 is :712, grace :759 is :844;
  Xapiand opts.cc moved one line (Xapiand:ad3f375b9: `MAX_CLIENTS` :50, `--max-clients` :185,
  `--tls-max-connections` :201). Prism, detentd's main and Xapiand's manager.cc and main.cc cites hold.
- Wrong: `--tls-ca` "turns on client authentication; the relay and the authority require it" (F1).
  C2's "only Xapiand checks the key and certificate": `configure` checks the match for everyone,
  at the note's own reactor:4de16f2 too (F10). "A main has seven steps" (F4).

### Question 3: one shape, or divergence without a reason?

The shape is one, but the note lets five differences through without a reason or a D62 case:

1. The 1 ms loops (F6).
2. `--processors` instead of `--reactors` (F7).
3. Flags versus config keys: the relay is JSON-only, while Prism and Xapiand have no config file (F7).
4. Bind-address defaults (F7).
5. Daemonizing and readiness: detentd and the relay use `run_daemon` with readiness, Xapiand forks
   with none, and Prism has none at all (F4).

The other rows are values (C4) or are removed by `Limits` and `stop_signals`, as the note says. No
deviation has a D62 measurement.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | `--tls-ca` is said to turn on client authentication, which "the relay and the authority require" (note:79,84,113). No app does that: `Role::Server` sets `verify_none` whatever CA it is given, and detentd and the relay both pass their CA to `Role::Server`. The relay authenticates clients by bearer-token credentials and uses its CA for its own outbound health and mirror clients. As written, adopting the flag either turns on mTLS and breaks token clients or adds a flag that does nothing. | reactor_tls.h:83-85; Detent detentd/main.cc:169, relay configuration.cc:151-157,204-208,269-273, relay main.cc:54 | State `--tls-ca` as the trust root for the server's outbound and peer connections. Drop "turns on client authentication" and the relay "Deviations" row. If mTLS for clients is wanted, make it its own option with a reactor `Role` change, decided by the owner. |
| F2 | yes | defect | The descriptor budget is "plain and TLS alike" (note:51-56), yet it lives in `tls::server_context(options, max_clients)` (note:49). So it vanishes when TLS is compiled out (`PRISM_TLS`, `XAPIAND_TLS`) and never runs for plain servers. "Must fit `RLIMIT_NOFILE`" at the default 1024 clients plus any reserve cannot fit a stock Linux soft limit of 1024 (systemd's default `1024:524288`) or macOS launchd's soft 256, unless the check first raises the soft limit toward the hard one, as Xapiand does today. Without that raise Prism and Detent (no rlimit code at all) refuse to start out of the box. The "stated reserve" is never stated. Xapiand's budget also sizes the database pool (7 descriptors per index) and `concurrent_search_indexes`, and it runs after `demote`, not before as the note says. | Xapiand main.cc:85-87,338-490 (setrlimit loop :375-436, after demote: :801 via `setup()` :814, demote :1118); tls.cc:54-58; Detent and Prism: no `RLIMIT_NOFILE`/`setrlimit` in src | Move the check out of TLS into reactor, for example `reactor::descriptor_budget(max_clients, reserve)`, called by every main before `demote`. Have it raise the soft limit toward the hard one first. Make the reserve an app value (Xapiand: `FDS_RESERVED` plus the pool's `FDS_PER_DATABASE` half, which stays Xapiand's). `server_context` then takes no `max_clients`. Test: a soft limit of 1024 with the hard limit above it starts. |
| F2b | no | gap | Refuse or shrink? Today Xapiand shrinks `max_clients` and `database_pool_size` to fit, with a warning (main.cc:455-464). "Must fit" reads as refuse, which narrows startup behavior. | main.cc:441-488 | Owner's call: (a) refuse with `EX_CONFIG`, one rule for three apps; (b) shrink with a warning, as Xapiand does today; (c) refuse only when the flag was given explicitly and shrink a default. The note should name the choice. |
| F3 | yes | defect | `Limits` duplicates two existing parameter sets. `AsioResponseOptions` already holds `request_timeout`, `idle_timeout`, `header_timeout`, `inactivity_timeout` and `max_body`. `Limits::tls_handshake`/`tls_handshakes` repeat `tls_connection_factory`'s arguments, while the proposed constructor still takes a separately built `ConnectionFactory` (note:37-39). That leaves two places to set the handshake bounds, against C1's "no setter can be forgotten" and D73's "one limits struct". | http_asio.h:92-107; http_asio_tls.h:368-373; note:30-39 | Say that `Limits` takes over `AsioResponseOptions`' limit fields (the response transforms stay where they are), and that the constructor takes the TLS context, building the factory from `Limits`, or that `tls_connection_factory(context, const Limits&)` is the only form. |
| F4 | yes | gap | "A `main` has seven steps" leaves out the process steps every server main has: daemonizing with readiness, the pid lock, the log session, the privilege drop and the data-directory lock. Step 5's `ready` is "daemon readiness" everywhere (note:61), but Xapiand's `detach()` forks first and the parent exits 0, so it has no readiness channel. `run_daemon`/`Ready`/`ProcessLock` live in Detent's app code, not a library (D73), and Prism has none of them. | Detent detentd/main.cc:154-155, relay main.cc:62-63, src/detent_cli/daemon.{hh,cc}; Xapiand main.cc:623-649,1015-1017,1118; prism src/main.cc:240-350 | Add a step 0, "Process", with its owner. Either `run_daemon` moves to a shared home (task, beside `stop_signals`, or a recorded D73 revisit), or each app keeps its own and the note says so with a reason. State what `ready` is for a foreground process and for Xapiand's `--detach`. |
| F5 | yes | gap | Step 4 orders the services but does not say how the handler and other code reach them. detentd hands its state service to http as the handler (main.cc:173-176). Xapiand's handler is a static, stateless `SearchService` that reaches everything through the global `XapiandManager::manager()`: about 190 references (test_node.cc 91, search_views.cc 14, handler.cc 9, index_resolver_lru.cc 4, schemas_lru.cc 3, plus `schedule_autocommit` from shard.cc:1594 and handler.cc:3331). With no rule, Xapiand's adoption keeps the global under a new name: two shapes, and the largest part of the adoption left unscoped. | Xapiand manager.h `make`/`manager`/`reset`; manager.cc:217-230 | State the rule: state services are constructed first and passed by reference to the services and handler that use them; a process global is a deviation with a reason. That sets the size of Xapiand's adoption (see the map). |
| F6 | yes | gap | The 1 ms loops in detentd and the relay are listed with "None measured; the one deviation that can earn one" (note:108). D82 keeps a deviation only with a D62 case, so as written this one is neither kept nor removed. | Detent PLAN D82; Xapiand PLAN D62; note:105-108 | Say that adoption moves them to wheel waits (`periodic` or a wake on progress), with Detent's adoption quest measuring throughput before and after (service-shape D1). They stay only if the unified shape costs over 5% CPU or 10% p99 after optimizing. |
| F7 | no | gap | Option names diverge with no deviation row. Xapiand keeps `--processors` for `--reactors`, with a reason (it also sizes the WAL writers and fsynchers) but against C3; it could take `--reactors`, defaulting from `--processors`. "Every limit is a flag and a config key spelled alike" cannot hold: the relay is JSON-only with four flags, and Prism and Xapiand have no config file. Bind-address defaults differ (loopback, or all for Prism and Xapiand) with no reason given. | note:24-26,81-85; Detent relay main.cc:13-35; Xapiand opts.cc:187-188,379 | Add the rows, or say which app gains flags or keys, and give the bind default's reason. |
| F8 | no | nit | Stale citations and the `PeriodicTimer` row (see Question 1). | above | Refresh at adoption time. The cited commits in the header already say where each line was. |
| F9 | no | nit | Sizing is stale: an idle connection now costs about 8.6 KB plain and 40.1 KB with TLS, so 1024 TLS connections are about 41 MB, not 100 MB. | http docs/perf/idle.md:72-80 (http:76f698d, http:ad6771f) | Update the numbers and cite idle.md. |
| F10 | no | defect | C2 says only Xapiand checks that key and certificate match. `configure` does that for every app. What only Xapiand adds is the key mode, the validity dates, the expiry warning and `strict = true` (no resumption, AEAD suites). The note does not say whether `server_context` sets `strict` for all three, which is a behavior choice for Detent and Prism. | reactor_tls.h:79 (same at reactor:4de16f2); Xapiand tls.cc:52-53,62,69-74 | Correct C2 and state `strict` in `server_context`. |
| F11 | no | gap | SIGUSR1 and SIGINFO dumps stay on an `asio::signal_set` (note:64), which never runs while the loop is wedged, exactly when a thread dump matters. service-shape D6 rejected `signal_set` for that reason. | service-shape.md D6; Xapiand manager.cc:595-600 | Run dumps from `stop_signals`' thread, or from a `sigaction` self-pipe read by a thread of its own. |
| F12 | no | nit | Decision IDs are ambiguous across three ledgers: Xapiand's D80-D82 are loop scheduling, Detent's are shutdown, connections and composition. The header omits Xapiand D71, D76 and D77, which the body uses. "Q4" in the deviations table points at a question now under "Settled". | note:10,108-110 | Prefix each ID with its ledger. |
| F13 | no | nit | Defaults move for Xapiand without saying so: `--max-clients` goes from 1000 to 1024, and per-loop in-flight goes from 1000 to 1024 unless Xapiand sets 1000 (D72: "the 1000-per-loop in-flight cap stays"). | Xapiand opts.cc:50; manager.cc:943,950 | State that Xapiand sets `max_in_flight = 1000` as a value, or that D72's 1000 becomes 1024. |
| F14 | no | gap | Settled 1 depends on "a detentd test that a held poll ... does not trip them", which is not named anywhere. | note:126-128 | Name it as an acceptance criterion of the http `Limits` quest. |

## manager.cc map

Seven steps: 1 options, 2 limits, 3 TLS, 4 loop and services, 5 `serve`, 6 signals, 7 housekeeping.
"Service" means a `task::service` under `serve`; "library" means it moves to task, http or reactor;
"app" means it stays Xapiand's as values or code; "not covered" means the note says nothing.

| Responsibility | manager.cc (and main.cc) | Step | Destination |
|---|---|---|---|
| Shutdown timing: grace 5 s, reserve 5 s, grace minimum 250 ms, `default_shutdown_write_bound` (DB_TIMEOUT 30 s + 5 s = 35 s), `http_shutdown_grace(left)` | 143-168; tests test_node.cc:862,1061 | 1, 2 | Library. `--shutdown-timeout-ms` maps onto `stop_timing::bound` and the grace is fixed in `Limits` (D77). The reserve and the adaptive grace are deleted (service-shape D7). |
| Control loop: `Reactors` (reactor, `AsioHost`, `control::Tasks`), `run_loop`, `break_loop`, `run_control`, `drain_control` 10 ms poll | 170-215, 540-567, 1006-1029 | 4 | Library: `asio_scheduler` on the main thread, `task::group` (join wakes at zero, service-shape D5). `control_plane.h` (322 lines) retires into task. The `engine::AsioHost` binding is app code and not covered. |
| Signals: self-pipe, `signal_sig`, `signal_loop`, `signal_sig_impl`, "you insisted", `sig_exit` | 186-214, 251-262, 459-498, 579-697; main.cc:106-300 | 6 | Library: `task::stop_signals`. The SIGUSR1/SIGINFO dump stays app (F11). |
| Shutdown driver: `shutdown`, `shutdown_impl`, `shutdown_async`, `shutdown_retry`, `shutdown_checks`, `try_shutdown`, `ready_to_end*`, `control_task_failed` | 184-186, 605-758, 1243-1256 | 5, 6 | Library, and deleted: `serve` plus `signals.fail()`. The retry goes (D80, step 7 "nothing polls"). |
| Ordered stop: `join()` stops 1-5, abandon `_Exit(EX_TEMPFAIL)`, teardown order; `unfinished_work` | 1032-1240; main.cc:908-930 `cleanup_manager` | 5 | Services. Http's drain (stop 1, `IndexChores::cancel_all`, `close`, `destroy_roots`), Autocommit's drain (stop 2), Registry's drain (stop 3), and its `flush()` (WAL and fsync, stops 4-5). `main` logs `served.unfinished` and exits `EX_TEMPFAIL`. `atexit(cleanup_manager)` is deleted. |
| State: `schemas`, `database_pool`, `wal_writer`, `index_settings_resolver`; `readers_per_index` | 265-310 | 4 | Services: Registry (pool, WAL writer, fsyncher) and SchemaCache. The resolver is plain state with no lifecycle. `readers_per_index` is an app value. |
| Housekeeping: `control::Periodic` 1 min, `housekeeping_tick`, `housekeeping_ticks`, `housekeeping_period` | 182, 501-516, 826, 981 | 7 | Library `task::periodic`, owned by the SchemaCache service. The tick is app code; tests read its count (test_node.cc:454,683-686). |
| Autocommit: `control::Debounce<Endpoint>`, `schedule_autocommit` | 183, 300-307, 533-537 | 7 | Library `task::debounce`, in an Autocommit service. Its callers (shard.cc:1594, handler.cc:3331) need F5's rule. |
| HTTP wiring: `make_servers` | 894-985 | 2, 3, 4 | Limits mapping (942-958) to `http::Limits`. TLS factory (926-947) to `server_context` plus the factory; the failure-log hook stays app. Bind options and `reuse_port` (901-908, 951-954), compression, `AccessLog`, lag probe (217-249, 956-957) are app values. `http_hosts`, one `AsioHost` per reactor (960-966), is not covered. Housekeeping start (981) goes to SchemaCache `start()` and fsyncher start (984) to Registry `start()`. `http_tries` (912) is dead. |
| Ready: `setup_node*`, `set_ready_impl`, `State::READY`, metric labels, callstacks snapshot, "Ready to Rock" | 852-1003, 1382-1397 | 5 | `ready` callback plus Http `start()`. `State::READY` is app (the tests wait on it, test_node.cc:463). The two log lines become one (note step 5). |
| Descriptor budget and TLS check | main.cc:338-490, 1105-1119; tls.cc:51-75 | 3 | Library for the client half and the TLS checks (F2). The database-pool half stays app. |
| Node identity: node name file, `host_address` (getifaddrs), `init` (preloads stopwords and MIME types, local `Node`), name check | 337-456, 761-824 | before 4 | Not covered. App: a node module `main` calls before `serve` (service-shape's map), with the preloads before any listener (D1). |
| Resolver forwarding: `resolve_index_settings`, `resolve_index_endpoints`, `invalidate_settings` | 330-334, 1259-1283 | - | Not covered. Callers use `IndexResolverLRU` directly, which needs F5. |
| Metrics: `server_metrics_impl`, `reactor_lag_metrics`, the `http_clients` counter (counts requests, not connections: search_views.cc:2755-2776) | 1286-1379 | - | Not covered. App: a function over the services' counters; the lag histograms come from reactor. |
| Global accessor: `_manager`, `make`, `manager`, `reset`, `get_state`, `spawn_control`, `control_lag` | 141; manager.h; 519-530 | - | Not covered (F5): about 190 references, half of them in the test harness. |
| Process: `detach`, `writepid`, `demote`, `usedir`, logging, banner, dump/restore modes (which run `setup()` without a server) | main.cc:623-760, 840-905, 992-1166 | - | Not covered (F4). Dump and restore need the Registry without `serve`. |

Adoption size and steps. About 1,700 lines of manager.cc and manager.h dissolve, along with about
350 lines of main.cc (signals, descriptor budget) and `control_plane.h`; test_node.cc's harness is
rewritten. It needs library work that has not landed: service-shape steps 1-3 in task, `Limits` and
close-idle in http, `server_context` and the budget in reactor. In order:

1. Move `control::*` onto task's `group`, `periodic` and `debounce`, with no behavior change.
2. Options, `Limits`, TLS and the budget: `--max-clients` becomes a bound (D72); add
   `--idle-timeout-ms` and `--shutdown-timeout-ms`; delete `--tls-max-connections`. Plain HTTP gains
   the bound and the idle close.
3. Apply F5's rule: services take their state by reference, and the global accessor and the test
   harness move with it.
4. Services, `serve` and `stop_signals`: Registry, SchemaCache, Autocommit and Http. Delete `join`
   and the shutdown machinery. QUIT calls `request_stop` and FLUSH?clients calls close-idle (D76).
5. Node identity, metrics and process steps, after F4 settles them.

Steps 3 and 4 are the large ones. 3 can precede 4 because it changes no behavior.

## Not run

- No probe jobs: no build or test was needed to settle these questions, and every finding cites a
  file and line.
- F2's stock limits (Linux soft 1024, macOS launchd soft 256) are the documented defaults, not
  measured: this host is raised to 1048576. A VM probe was not run because the VMs belong to Detent.
- I did not check Detent's authority service for wheel-readiness, or `managed_host.cc` (detent-agent
  shares only the signal, housekeeping and stop steps).
- I did not check whether the "held poll" test in Settled 1 exists in Detent's tests (F14).
