# track-c: Track C, the consensus gaps after C1

Risk: low (a roll-up of landed, reviewed quests; D123, D125). The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-c`.

## Outcome

Track C closes. Each item it owed landed in a quest of its own, reviewed by its own risk, in
Kronuz/consensus, Kronuz/journal or strata (strata `docs/design/track-c.md`, "Reconciliation
(2026-11-18)", and decisions D123 and D125). This quest records where each item lives, so
`cluster-gate` and Phase 3R build on a closed track. It changes only strata's track documents.

## Acceptance criteria

Each row names the landed quest and its commit, quoted from the document it cites.

- A1 [artifact] C1's owed points: conditions 22 and 23 (consensus:b7c527f); the hello's limits and wire version range (`consensus-rpc-lane`, F3 of replan-2026-11-20); the `DTC2` commit as Detent's pin move (`detent:detent-on-drivers`): `docs/design/track-c.md`
- A2 [artifact] C2: T2 over shared turn logs on hosts of 2 to 4 loops (`consensus-sim-loops`, consensus:8bdbd750a), and the quiescence rows (`detent:consensus-quiescence`): `docs/gates/track-c-c2.md`
- A3 [decision] SYS-8 (a): the shared turn log is the journal backend, and T6 is a measurement row after the first release: D125
- A4 [artifact] C3's gate rows and the architect's review (`track-c-c3-gate`, strata:7cd834b3d): `docs/gates/track-c-c3.md`
- A5 [artifact] C4: D16's multiplexing and credit (`detent:transport-groups`, `detent:transport-identity`), TLS (`detent:transport-connection`), T7's rows and the control-frame gap that replaces R19's bound (`consensus-flow-bench`, consensus:122564d61; D123), and where D18's retry from a newer checkpoint and its third-failure stop are built or carried: `docs/gates/track-c-c4.md`
- A6 [artifact] C5: the simulator and linearizability checker, carried by group-heal (group-sim was dropped): `docs/mechanisms/simulator.md`: "linearizability checker judges it"
- A7 [artifact] SYS-2 and SYS-3 (a): `peer-transport.md` is marked superseded by the landed host transport, with its unbuilt requirements named where each is carried (D125): `docs/design/peer-transport.md`
- A8 [artifact] Goal 5's audit (`goal5-audit`, consensus:ac323fe6e, `docs/GOAL5-AUDIT.md` in consensus), before `cluster-gate` (D123): `docs/design/track-c.md`
- A9 [artifact] A gate report per item: C2 completed and C4 written (`track-c-gates`): `docs/gates/track-c-c4.md`

## Contracts

None of its own: the contracts live in the quests above. The `DTC2` wire format is shared with
Detent and stays open until `format-freeze` (SYS-3).

## Decisions

- D123 (lead): `cluster-gate` lands after `goal5-audit`; R19's 64 KiB bound is replaced by the bound
  the host transport states, proven by T7's control-frame gap.
- D125 (owner): SYS-8 (a) and SYS-2/SYS-3 (a), as A3 and A7 state.
- Risk is low: the branch changes documents only, and every mechanism was reviewed in its own quest.

## May land early

Nothing: it lands once, after `track-c-gates`.
