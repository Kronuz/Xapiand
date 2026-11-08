# automata: Prefix, wildcard and fuzzy term matching where search lives

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status automata`.

## Outcome

Prefix, wildcard and fuzzy (edit distance) queries expand over prospect's term dictionary through
automata, ported from strata's `phase5/automata` to where `automata-evaluation` places each part, so
Xapiand serves `OP_WILDCARD`, `OP_EDIT_DISTANCE` and `QueryParser`'s wildcard, partial and fuzzy flags
without Xapian. It lands only if `automata-evaluation`'s ruling says to port; otherwise the lead drops
it with that ruling as the reason.

## Acceptance criteria

From strata `docs/design/automata.md` revision 1 and `docs/PLAN.md`, Phase 5.

- A1 Prefix, wildcard and fuzzy expansion equal a brute-force scan of the term dictionary on every
  segment shape the design's T1 to T3 name: tests in jobs
- A2 One CPU-hour of `fuzz_automata` with zero findings; ASan, UBSan and TSan builds pass the tests:
  jobs
- A3 The prefix, wildcard and fuzzy classes measured against Xapian and against the faster of Lucene
  and Tantivy (Phase 5's gate rows), on the gate's machine: jobs
- A4 [artifact] How it works, in the repository the port lands in: `docs/mechanisms/automata.md`

## Contracts

Provides `prospect:term-expansion`.

## Decisions

1. Needs `automata-evaluation`; its ruling decides the repository and the file set before start.
2. The phase5 quest's A2 is met by this quest's landing.

## May land early

None.
