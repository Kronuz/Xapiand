# e2e-harness: journal

State: ready for review
Quest: .campaign/xapiand/quests/e2e-harness.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: ready to land (low risk: checks green, the lead reads the diff).

## Next

1. Land.

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | probe check 261108-225615-8a07ff at Xapiand:029b324e1 (a binary that exits 1: `run_all.sh e2e` exits 1) |
| A2 | probe check 261108-225615-8a07ff (`engine_e2e.sh` exits 1 when the capture fails) |
| A3 | probe check 261108-225615-8a07ff (`RUN_ALL_STRICT=1 run_all.sh tls` without newman 6 exits 1) |
| A4 | manual, the lead 2026-11-08 22:55: at master (Xapiand:6756e1be9 tree, the main checkout) `BIN=<a binary that exits 1> harness/run_all.sh e2e` exits 0, VERDICT GREEN, graded from the stale /tmp/xa_e2e_ours.json of 2026-11-04 (the revert check: the same probe exits 0 there, where it must exit 1) |

## Result

The docs E2E fails when its capture fails: a fresh report per run in run_all.sh, engine_e2e.sh's exit status, RUN_ALL_STRICT=1 for gate checks (harness/run_all.sh, harness/engine_e2e.sh). Every earlier E2E pass on this host with newman 4.5 first on PATH is void.

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
