# lua-method-flake: journal

State: waiting on jobs 261121-023345-2ed0fe 261121-023348-4964f7 261121-023351-5378d5 261121-023355-74e637
Quest: .campaign/xapiand/quests/lua-method-flake.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix committed (Xapiand:03b4e6f9c); A1/A2 jobs and a master load probe queued; branch pushed for A3.

## Next

1. Read the four jobs: A1 master job must fail with the CI signature (test_lua.cc:1673 ...); fix job passes; A2 50/50.
2. A3: CI green on every platform at the branch head.

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
| A1 | 261121-023345-2ed0fe (master, --fails-without, listen delayed), 261121-023348-4964f7 (fix, same job) |
| A2 | 261121-023351-5378d5 (50 runs, 8 at once, --cpus 4, at the fix) |
| A3 | CI at the branch head (pushed) |

Probe (not evidence): 261121-023355-74e637, the A2 load at master, to see whether load alone reproduces it on macOS.

## Result

tests/utils_node.h (new): `test_utils::ReservedPort` (a bind(0) port kept bound with SO_REUSEPORT beside the
node's listener on Linux/FreeBSD, released at once on macOS where the node binds without it) and
`test_utils::connect_to` (connect retried up to 5 s, the mechanism test_node.cc and test_endpoint_uuid.cc use).
tests/test_lua.cc MethodNode: ready only when a connect succeeds; every call connects through connect_to; port from
ReservedPort.

## Rulings I made

- Test fix, not node fix: READY before the listener is outside this quest's file set (src/manager.cc); other
  tests already work around it with connect_to. Recommendation for the lead: a follow-up to set READY after
  `http_service->start()`, or migrate the other copies of free_port/connect_to (test_node, test_endpoint_uuid,
  test_loop_yield, test_coverage_database, test_stopword_search) to tests/utils_node.h. Out of my file set.
- macOS keeps a small port window (release, then the node binds); a clash there fails the node's bind loudly.
