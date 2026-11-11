# e2e-report-cap: journal

State: waiting on jobs 261111-013408-376057 (repro, informational), 261111-015200-ea9eea (A1, A2 at fac7e9dd0)
Quest: .campaign/xapiand/quests/e2e-report-cap.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix committed (fac7e9dd0): e2e_capture.sh writes a `slim` newman reporter beside the run (NODE_PATH) that
keeps the report's shape but a response body only (first 2800 bytes) for a request with a failed
assertion; e2e_assert.py is unchanged. Local check against a fake server: one newman run with both
reporters gave a 53 MB full and a 9.2 MB slim report (600 failing requests, bodies kept) and
e2e_assert.py printed byte-identical output on both. Job 261111-015200-ea9eea (exp2.sh) runs the real
E2E at the head: A1 (GREEN, size under 5 MB) and A2 (a newman shim breaks every default expectation;
assert must be NOT GREEN, rc 1). Job 261111-013408-376057 (exp1) reproduces the full report from main
to see why it did not parse (informational).

## Next

1. Read both jobs; fill Evidence from `campaign brief`; set State ready for review (low risk).

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Cap by dropping passing bodies in a slim reporter, not by editing e2e_assert.py: the assert's inputs (status, assertion errors, request, failing body) are unchanged. E2E_BODIES=1 keeps the stock reporter for e2e_diff.py/bodydiff.py, which compare bodies.
- Reporter is written by e2e_capture.sh into its work dir (file set is the two harness files).

<Decisions inside the quest's scope, so the reviewer can check them.>
