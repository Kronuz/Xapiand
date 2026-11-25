# track-c: journal

State: ready for review (strata:be00906f)
Quest: .campaign/xapiand/quests/track-c.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: the documents are written and committed; waiting for review.

## Next

1. Review `docs/design/track-c.md` "Rulings (2026-11-25)" and `docs/design/peer-transport.md` (strata:be00906f).

## Blocked on

Nothing blocks the rows. Two notes for the lead:

1. A6: `docs/gates/phase-3r.md` is a STUB at strata:be00906f ("Status: STUB (2026-11-23) on quest `group-heal`"; its gate rows, 10^7 steps over 1,000 seeds, "are the lead's, written when they run"). The quest `group-sim` is dropped; `group-heal` carries the simulator. A6's text is met only when those rows are written.
2. The A7 files (`docs/design/peer-transport.md`, `docs/design/track-c.md`) are in the quest's file set; no file outside it changed. The node directory, admin path and D5/D18 replica checks (peer-transport item 4) have no quest: `cluster-membership` and `placement-root` are dropped. The lead files them or points them at Track D.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/track-c.md` (strata:be00906f), "Reconciliation" row A1 and Rulings 5: conditions 22 and 23 `consensus:b7c527f` (landed, `de99ec7e`); hello limits and version range in `consensus-rpc-lane`, `consensus:e1f249d` ("`consensus-hello-limits` ... Dropped, folded into `consensus-rpc-lane`", `docs/gates/track-c-c4.md`); `DTC2` is the pin move in `detent:detent-on-drivers` (not landed) |
| A2 | `docs/gates/track-c-c2.md` (strata:45b0ab43): T2 over shared turn logs at 2 to 4 loops, `consensus-sim-loops` landed `consensus:8bdbd750a`; quiescence rows are `detent:consensus-quiescence` (not in this campaign's landed list). Report heading: "T9 fails at `journal:178030b`" (Detent msg-183) |
| A3 | D125 (Xapiand PLAN): "SYS-8 (a), the shared turn log is ratified as the journal backend, and T6 ... moves after the first release"; recorded in `docs/design/track-c.md` Rulings 1 |
| A4 | `docs/gates/track-c-c3.md` and `docs/reviews/track-c-c3.md`, `track-c-c3-gate` landed strata:7cd834b3d (review accepted at strata:5cab3e7b) |
| A5 | `docs/gates/track-c-c4.md` (strata:560e8851): T7 rows, R19 row ("`election_min`: pass at both commits, every row at least 16 times under it (worst 59.0 ms)"; the stated 64 KiB-class bound: miss), `consensus-flow-bench` landed `consensus:122564d61`; `detent:transport-groups`/`transport-identity`/`transport-connection` = `consensus:e4e394b`, `361d5d2`, `677c864`; D18 third-failure stop and connection-wide window: "unbuilt", named in the report |
| A6 | `docs/gates/phase-3r.md` (strata:e256792b, STUB; see Blocked on 1) |
| A7 | `docs/design/peer-transport.md` (strata:be00906f), first line: "Status: SUPERSEDED (D125, 2026-11-25) by the landed host transport"; unbuilt requirements 1 to 4 named there |
| A8 | `docs/GOAL5-AUDIT.md` in consensus, `goal5-audit` landed `consensus:ac323fe6e`; order ruled D123 in `docs/design/track-c.md` Rulings 3 (strata:be00906f) |
| A9 | `docs/gates/track-c-c4.md` written and `docs/gates/track-c-c2.md` completed, `track-c-gates` landed strata:560e8851d (reviews `docs/reviews/track-c-c2.md`, `track-c-c4.md`: APPROVE WITH CONDITIONS); C1 `docs/gates/track-c-c1.md` strata:de99ec7e |

## Result

strata:be00906f: `docs/design/track-c.md` "Rulings (2026-11-25)" (D125, D123, R19) with the decisions list marked resolved; `docs/design/peer-transport.md` marked superseded with its unbuilt requirements named; main merged (strata:5368a6fa). No jobs.

## Rulings I made

- Merged strata main into the branch (strata:0b4893be), as the brief asked; the two add/add conflicts were main's PARKED/HISTORY labels on otherwise equal files, resolved to main's.
- Updated `docs/design/peer-transport.md`'s label: revision 2 passed both re-checks, and the reconciliation names the decision.
