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

- A1 The cause, named with evidence: a job that reproduces the failure on master (for example
  test_lua beside a parallel ctest run or under load), and the same job passing at the fix
- A2 test_lua passes 50 runs in a row, 8 at once on 4 cores, at the fix: a job
- A3 [ci] CI is green on every platform

## Contracts

None.

## May land early

Nothing: one landing.
