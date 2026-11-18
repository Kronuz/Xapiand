# track-c: journal

State: blocked on owner and lead: SYS-8, SYS-2/SYS-3 (A7), A8 order, R19, quest text
Quest: .campaign/xapiand/quests/track-c.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: the design assignment (reconcile A1-A9 with what landed) is done at strata:f2358007.

## Next

1. The owner rules SYS-8 (A3) and SYS-2/SYS-3 (A7); the lead rules A8's order, R19, and re-plans the quest's criteria and the proposed quests (strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)").

## Blocked on

The five decisions in that section's last list, each with options and a recommendation: SYS-8 and A3 (owner), SYS-2/SYS-3 and A7 (owner), A8's order (lead), R19 (lead), the quest's text and group-install's contract (lead).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | owed in part: conditions 22/23 done (consensus:b7c527f); hello limits owed (proposed consensus-hello-limits) |
| A2 | owed in part: consensus:235f189, consensus:0455f58; multi-loop T2 owed (consensus-sim-loops); quiescence rows in detent:consensus-quiescence |
| A3 | obsolete as written (cluster-release section 4; Detent D97 Q3 (b)); owner decision |
| A4 | owed (track-c-c3-gate) |
| A5 | owed in part: consensus:e4e394b, consensus:361d5d2, consensus:677c864; T7 owed (consensus-flow-bench) |
| A6 | covered by group-sim |
| A7 | re-checks done (strata:38a6c6f3, strata:88b94e39), in conflict with the landed DTHL0002; owner decision |
| A8 | owed (goal5-audit) |
| A9 | owed (track-c-gates); C1 passed at strata:de99ec7e |

## Result

strata `docs/design/track-c.md`, "Reconciliation (2026-11-18)" (strata:f2358007): a table of A1-A9, six proposed quests, five decisions. Read only; no jobs.

## Rulings I made

- Merged strata main into the branch (strata:0b4893be), as the brief asked; the two add/add conflicts were main's PARKED/HISTORY labels on otherwise equal files, resolved to main's.
- Updated `docs/design/peer-transport.md`'s label: revision 2 passed both re-checks, and the reconciliation names the decision.
