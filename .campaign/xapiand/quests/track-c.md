# track-c: Track C, the consensus gaps after C1

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status track-c`.

## Outcome

Track C's remaining items pass their gates, so Phase 3R can start: C2 (multi-group operation on one
journal per core), C3 (repair at or below the commit cursor), C4 (stream transport with flow control,
TLS before production) and C5 (a deterministic simulation harness with a linearizability checker), plus
C1's owed points and the peer transport design (SPKI pins, an N-1 version range). The code lands in
Kronuz/consensus and Kronuz/journal, each step a quest of its own in that repository ordered by
`after`; this quest holds the design, the gate reports and the reviews, in strata.

## Acceptance criteria

From strata `docs/PLAN.md` "Track C" (items C2 to C5, "Gate per item": tests and measurements in the
`consensus` and `journal` repositories, reviewed by the architect) and "Owner decisions from the system
review" (SYS-2, SYS-3, SYS-8), with what `docs/design/track-c.md` records as still owed.

- A1 [artifact] C1's owed points: the hello's limits (`DTPROF02`), the transport sites following the node's limits, and the coordinated `DTC2` commit with Detent's pin move, with the review's conditions 22 and 23: `docs/design/track-c.md`
- A2 C2: T2's oracles over a shared journal (multi-voter groups on several cores' turn logs), and the D14 prototype's quiescence rows: jobs in consensus and journal
- A3 C2 and SYS-8: T6, fsyncs per second per node at 1,000 groups for the per-group and the shared journal backends, decided with Detent's maintainer before either backend is removed or built on: exclusive jobs on the bare-metal host
- A4 [artifact] C3: its gate rows and the architect's review (built in consensus afd456d without a gate report): `docs/gates/track-c-c3.md`
- A5 C4: D16's multiplexing of groups onto node-pair connections and its credit, R19's unsent-bytes bound and its T7 row, D18's catch-up retry from a newer checkpoint, and C4's gate rows; TLS before production: jobs in consensus
- A6 C5: a deterministic simulation harness with a linearizability checker, designed with Track D's D1 under one owner (strata `docs/design/simulator.md`): jobs
- A7 [artifact] The peer transport design (Track C's D16 extended with SPKI pins and an N-1 wire version range, SYS-2 and SYS-3) passes the architect's and the security reviewer's re-checks of revision 2: `docs/design/peer-transport.md`
- A8 [artifact] Goal 5's audit of `completion`, `journal` and `consensus` (no tracing collector, no unbounded deferred reclamation), before Phase 3R adopts them: `docs/design/track-c.md`
- A9 [artifact] A gate report per item, C2 and C4 completing the reports in progress: `docs/gates/track-c-c2.md`

## Contracts

Consumes Detent's split of `completion`, `journal` and `consensus` and its peer transport (strata
`docs/PLAN.md`, Track C ordering); the wire format `DTC2` is shared with Detent and is not frozen
before Track C finishes it (SYS-3).

## Decisions

- Parked by the owner (2026-10-20, D4): distributed work parks behind engine speed; then the
  single-node release ships first and 7b follows it (2026-10-29, D9). Blocked until the owner resumes
  distribution.
- Detent owns the journal and consensus coroutine drivers (Detent milestone M1b), with Xapiand's four
  requirements binding (D21, Detent:a2d999a): the lead reviews each design before code.
- Adopts `design/peer-transport` (strata:d8384c8df: revision 2, awaiting its re-check; the architect's
  PT-R1 to PT-R26 and the security reviewer's SR findings in `docs/reviews/peer-transport-design.md`
  and `docs/reviews/peer-transport-security.md`).
- Prior evidence, not to redo: `docs/gates/track-c-c1.md`, `docs/gates/track-c-c2.md` (T8 and T9 for
  C2's formats, journal a678e44), the C4 progress table (D15, D18, R19 frame cap on consensus main).
- Open (owner): the bare-metal host for A3 (PLAN open question 9).

## May land early

The design and report documents.
