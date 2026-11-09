# automata-evaluation: Evaluate strata's automata and decide where it belongs

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status automata-evaluation`.

## Outcome

A decision, on numbers, about strata's `phase5/automata` (strata:219ded0d7, 2026-10-18): prefix,
wildcard and fuzzy term matching by automata intersected with the term dictionary (strata
`docs/design/automata.md` revision 1, its review's AUT-R1 to AUT-R14, `docs/status/automata.md`). It
was written in strata two days before the 2026-10-20 split moved search to prospect, so it cannot
land in strata as it is. The evaluation says what the branch delivers and still lacks, whether it is
worth porting (useful for Xapiand's wildcard and fuzzy queries, and efficient), and where each part
belongs now: prospect for the expansion and its scorer hand-off, or a library (termdict, fst) for the
automata and the intersection. The owner's ruling (D37): take it up, evaluated at least, and ported
where it belongs if it is worth it.

## Acceptance criteria

- A1 [artifact] The evaluation: the branch's design decisions and tests, what the split moved, the
  port's cost, where each part belongs, and the branch's tests and a brute-force check run as jobs at
  a port of its tests or at the branch: `docs/design/automata.md`
- A2 [decision] The ruling it leads to (port, where, or not): a row in PLAN.md's Decisions

## Contracts

None: it changes no code.

## Decisions

1. Source: strata `phase5/automata` on GitHub (strata:219ded0d7), with its design, review and ledger.
2. The evaluation lands in prospect's `docs/design/` beside the search code it would join; the
   `automata` quest ports it only if A2 says so.

## May land early

None.
