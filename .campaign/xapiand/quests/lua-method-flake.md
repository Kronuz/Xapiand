# lua-method-flake: test_lua's method case passes every run, under load and beside parallel tests

Risk: low (a test). The lead writes this quest; contributors read it and never edit it. Its machine
state is in `state.json`: `campaign status lua-method-flake`.

## Outcome

`test_a_script_reads_the_method_of_its_write` (tests/test_lua.cc, landed with script-method,
Xapiand:4360188de) fails nondeterministically: every `_method` call in a run fails (from
test_lua.cc:1673), while master passes the same code. Seen on 11-21 in three runs: Linux on
quest/xapiand/strata-pin-catalog (run 44886765917, test 29.46 s) and cjk-words (44887030318,
54.03 s), and FreeBSD on bulk-load-rerun (44889113706, a docs-only branch). Suspects: MethodNode
waits at most 30 s for READY and the calls then run against a node that is not ready; or
`free_port()` picks a port that a parallel ctest test binds first. Find the cause and fix the test
(or the code, if the node is at fault), so it cannot fail for either reason.

## Acceptance criteria

- A1 [decision] The cause, named: READY set before the HTTP listener accepts (D163, the owner's
  ruling to land on probable cause; the reproduction is the hunt-list row "lua-method-flake cause")
- A2 test_lua passes 50 runs in a row, 8 at once on 4 cores, at the fix: a job
- A3 [decision] READY means the HTTP listener accepts: `set_ready_impl` (src/manager.cc) starts the
  listener before it sets READY, so a client that connects once after READY, with no retry, is never
  refused (D163; test_node's test_a_ready_node_accepts_its_first_connection passes at the fix, job
  261121-024312-003829, but its fails-without run passed too)
- A4 One node-test helper: every test that picks a port or connects to a test node (test_coverage_database,
  test_endpoint_uuid, test_loop_yield, test_lua, test_node, test_stopword_search) uses
  tests/utils_node.h; no other free_port or connect_to copy remains
- A5 [ci] CI is green on every platform

## Decisions

- Lead, 2026-11-21: fix the cause in manager.cc, not only the test; src/manager.* is in cluster's
  file set, but quest/xapiand/cluster has no change to it, so this quest takes it by a recorded exception.

## Contracts

None.

## May land early

Nothing: one landing.
