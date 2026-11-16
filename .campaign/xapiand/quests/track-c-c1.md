# track-c-c1: Track C's C1, membership changes and voter roles

Risk: high. History: this landed before the campaign, under the earlier campaign skill. The code is in Kronuz/consensus on its main; the gate report and the architect's approval are in strata, recorded at strata:de99ec7e (2026-10-14, the sixth revision check: "C1's gate: APPROVE WITH CONDITIONS"). No ledger existed then: strata's `docs/PLAN.md` "Track C", `docs/design/track-c.md`, `docs/gates/track-c-c1.md` and `docs/reviews/track-c-design.md` are the record. Its machine state is in `state.json`: `campaign status track-c-c1`.

## Outcome

`consensus` changes membership through its log: single-voter steps and replacements, quorums that follow the current membership, incarnations, leadership transfer, pre-vote with the guard and CheckQuorum, learners and witnesses. Detent's pinned formats still recover, the old reader refuses every new one, the randomized simulation holds its safety oracles, liveness holds in simulated time, and every new decoder is fuzzed.

## Acceptance criteria

Carried from strata `docs/PLAN.md`, "Track C", item C1 and its "Gate per item" (tests and measurements in the `consensus` repository, reviewed by the architect), with the rows of `docs/design/track-c.md`, each shown by the gate report or the review as an artifact.

- A1 [artifact] C1: membership in the log, one voter at a time with replacements, quorums that follow the current membership, incarnations, leadership transfer, pre-vote, the guard and CheckQuorum (design D1 to D7): `docs/design/track-c.md`
- A2 [artifact] C1: learners and an optional witness (design D8, D9): `docs/design/track-c.md`
- A3 [artifact] Gate T1 compatibility: Detent's pinned fixtures recover with today's code, and the pinned reader refuses each format Track C added: `docs/gates/track-c-c1.md`
- A4 [artifact] Gate T2 safety: the randomized simulation with T2's fault mix and every oracle T2 lists: `docs/gates/track-c-c1.md`
- A5 [artifact] Gate T3 liveness in simulated time, 3 and 5 voters: `docs/gates/track-c-c1.md`
- A6 [artifact] Gate T4 learners and witnesses: `docs/gates/track-c-c1.md`
- A7 [artifact] Gate T8 robustness: every new decoder fuzzed one CPU-hour, zero crashes; coverage to the targets: `docs/gates/track-c-c1.md`
- A8 [artifact] The architect's review of the item approves: `docs/reviews/track-c-design.md`
- A9 [artifact] The review's conditions 22 and 23, due before the coordinated `DTC2` commit: `docs/reviews/track-c-design.md`

## Contracts

The storage formats `RFTINIT2`, `RFTJOIN1`, `RFTBTCH2`, checkpoint bundle v3, `RFTSNP02` and wire tags 1 to 8, in Kronuz/consensus; the coordinated `DTC2` commit with Detent's pin move is still owed (quest `track-c`).

## Decisions

- The architect approved the Track C design with required changes R1 to R15, its revision checks added R16 to R20; C1 went through six revision checks, the fourth and fifth CHANGES REQUIRED, the sixth APPROVE WITH CONDITIONS (2026-10-14).
- The bare-metal M1 was retired on 2026-10-14; C1's reruns ran at consensus faef7ad on the macOS host.
- Risk high: consensus safety and persistent formats shared with Detent (this audit's mapping).

## May land early

None.
