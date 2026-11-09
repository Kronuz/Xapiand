# Efficiency ledger: campaign skill against the earlier gated pipeline

Started 2026-11-08 21:30 PST, weekly quota 0% (reset 2026-11-16 05:00 UTC). From agent-campaign PR #1
(usage-claude-code) `campaign usage` reads Claude Code; until it merges, run that branch's
`scripts/campaign.py usage`. Two token units: `usage` counts cumulative tokens per request (cached reads
included); a completion notice counts the agent's final total. Compare like with like.

Baseline (the earlier gated pipeline, week of 2026-10-26, from the skill's measured costs): explorers
~85k tokens each; developers 130-460k; reviews 150-400k; a repin 74k fresh against ~500k resumed.
Lead turns are not counted on either side.

Measures per landing: agent tokens, weekly quota points, wall time start to land, review rounds,
rework (fix rounds, relaunches), jobs and their machine time.

## Agent runs

| Date | Quest | Role, model, effort | Tokens | Quota before → after | Wall | Outcome |
|---|---|---|---|---|---|---|
| 11-08 | x-pool | contributor, opus, high | 363k (notice); 44.5M cumulative at 1h15m, 99% cached | 0% → 2% at 22:34 (all work) | 1h27m, 228 tool calls | RESTORE E2E ruled host overload (R1); built map_owed, assertions, clamps, design rev 3, mechanism note; found the E2E checks prove nothing; waiting on 7 jobs |
| 11-08 | wbuf-inline | contributor, opus, high | 152k (notice) | 2% → | 22m, 69 tool calls | rebased clean onto strata:70df28d; job scripts for every criterion; waiting on 10 jobs |

## Lead-only work (no agent)

| Date | Quest | What | Jobs |
|---|---|---|---|
| 11-08 | e2e-harness | The E2E harness fails when it proves nothing (Xapiand:029b324e1) | 2 |
| 11-08 | wbuf-inline | A11: deadline wait (strata:47628b2), repeat harness (strata:c15019c, strata:287b461) | 4 (2 failed on the VM key, rerun); A11 done: branch 0/300, main 49/300 |

## Incidents

| Date | What | Cost |
|---|---|---|
| 11-08 | Migrated checks built with -j8/-j6 under light holds; load 126 on 16 CPUs blocked Detent's strict jobs | x-pool's first check round (build-test, e2e, asan green; tsan red at 3f4114f) is keyed to the old commands and reruns; fixed in 37 quests (Xapiand:ca85dd0c9 and the others) and in the tool (agent-campaign:aa627ea) |
| 11-08 | Every `e2e` check graded a stale /tmp report as GREEN (newman 4.5 on the jobs' PATH); found by the x-pool contributor | every earlier E2E pass on this host is void; e2e-harness fixes it, 14 quests' E2E checks now set newman 6 and RUN_ALL_STRICT |

## Landings

| Quest | Started | Landed | Agent tokens | Quota points | Review rounds | Fix rounds |
|---|---|---|---|---|---|---|
