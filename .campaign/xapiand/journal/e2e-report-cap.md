# e2e-report-cap: journal

State: ready for review (A1, A2 at fac7e9dd0)
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
assert must be NOT GREEN, rc 1). Job 261111-013408-376057 (exp1, done): the full report from main is 64.0 MB and parses
OK with the same Python 3.15.2, so the earlier JSONDecodeError (char 62889311 of 63.95 MB) did not
repeat; its cause stays unexplained (a truncated or still-written file, or a parser limit at this size).
The size is the exposure the slim reporter removes.

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
| A1 | 261111-015200-ea9eea at fac7e9dd0: passing report 1,773,356 bytes (was 64.0 MB), e2e_assert.py GREEN rc=0 |
| A2 | 261111-015200-ea9eea at fac7e9dd0: a shim breaking every default expectation gives NOT GREEN, rc=1 |

## Result

The docs E2E report went from 64.0 MB (exp1 job 261111-013408-376057, stock reporter on main) to 1,773,356 bytes
(job 261111-015200-ea9eea at fac7e9dd0) with the assert GREEN. Cause: newman's json reporter writes each response body as a Buffer,
one number per line (~19 bytes per body byte). Fix: harness/e2e_capture.sh writes a `slim` reporter that keeps a body (first 2800
bytes) only for a request with a failed assertion; e2e_assert.py is unchanged; E2E_BODIES=1 keeps the stock reporter for
e2e_diff.py and bodydiff.py. The earlier JSONDecodeError (char 62889311 of 63.95 MB) did not reproduce: the 64.0 MB report
parsed with the same Python 3.15.2, so its cause stays unexplained; the size is the exposure the slim reporter removes.
Open: nothing.

## Rulings I made

- Cap by dropping passing bodies in a slim reporter, not by editing e2e_assert.py: the assert's inputs (status, assertion errors, request, failing body) are unchanged. E2E_BODIES=1 keeps the stock reporter for e2e_diff.py/bodydiff.py, which compare bodies.
- Reporter is written by e2e_capture.sh into its work dir (file set is the two harness files).
