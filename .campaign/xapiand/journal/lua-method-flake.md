# lua-method-flake: journal

State: waiting on jobs 261121-023345-2ed0fe 261121-024310-c2672c 261121-024311-504e9e 261121-024312-003829 261121-024314-671378 261121-024354-89c85f
Quest: .campaign/xapiand/quests/lua-method-flake.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead ruling (fix the cause, A3-A5) done: Xapiand:2e2a2307e (manager), Xapiand:2906afb92 (tests on utils_node.h,
the A3 test). Jobs queued; branch pushed for A5.

## Next

1. Read the jobs: 2ed0fe (master) and 504e9e (mutant) must fail at the expected check; the rest pass.
2. A5: CI green on every platform at the head.

## Blocked on

Nothing.

## Cause (A1)

`XapiandManager::set_ready_impl` (src/manager.cc) sets READY, then `http_service->start()` binds and listens.
MethodNode took READY as ready, so its first request (the PUT, test_lua.cc:1673) could be refused (status 0).
All three CI failures (attempt-1 jobs 136793672793, 136794427702, 136800821830) show the same 9 checks, all
cascades of that one refusal: GET/PATCH/UPDATE of the missing `put`, and GET `restored` (the POST, now the
index's first write, typed `_id` from an auto UUID, so the RESTORE line is dropped silently, cf. D162). A port
clash would instead split connections between two SO_REUSEPORT nodes at random, not hit the first request in all
three runs. Repro: `delay_listen.c` (~/.scratch/xapiand/lua-method-flake/, DYLD-interposes listen() +500 ms).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-023345-2ed0fe (master, --fails-without, listen() delayed 500 ms), 261121-024310-c2672c (head, same job) |
| A2 | 261121-024314-671378 (50 runs, 8 at once, --cpus 4, at head) |
| A3 | 261121-024311-504e9e (mutant 2906afb92 = head without the manager fix, --fails-without), 261121-024312-003829 (head); test_node's test_a_ready_node_accepts_its_first_connection under the shim |
| A4 | 261121-024354-89c85f (grep: no free_port/connect_to/::connect copy in tests/*.cc; the six node tests pass, ctest -j4) |
| A5 | CI at the head (pushed) |

## Result

- src/manager.cc `set_ready_impl`: `http_service->start()` before `exchange_state(READY)`. start() returns once
  every reactor's acceptor listens (reactor.h waits on each AcceptControl), so READY implies the listener accepts;
  a bind failure ends setup before READY. Nothing in src reads READY to gate requests.
- tests/utils_node.h: `test_utils::ReservedPort` (bind(0) port held with SO_REUSEPORT beside the node's listener on
  Linux/FreeBSD; released at once on macOS, where the node binds without it) and `test_utils::connect_to` (one
  attempt). Used by test_coverage_database, test_endpoint_uuid, test_loop_yield, test_lua, test_node,
  test_stopword_search; their copies and connect retries are deleted.
- test_node: `test_a_ready_node_accepts_its_first_connection`, first case after the process's node is READY.

## Rulings I made

- connect_to keeps no retry anywhere: every connect in the six tests targets a node already READY (in-process or in
  its own forked child); no test connects to a node it is starting or stopping.
- test_node's taken-port test binds its own listener on a bind(0) port instead of a free_port: a held reservation
  (SO_REUSEPORT) would block its plain bind.
- macOS keeps a small port window (released, then the node binds); a clash there fails the node's bind loudly.
