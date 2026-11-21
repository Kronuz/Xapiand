# split-enum-reflection: journal

State: waiting on jobs 261121-130303-a6714d 261121-130303-02426f 261121-130315-050173 261121-130318-1c55be
Quest: .campaign/xapiand/quests/split-enum-reflection.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Result: no split. `enum.h` stays one header; `docs/source-layout.md` (enum-reflection:6227a07)
says why. Waiting on the jobs above.

## Next

1. When the jobs pass and CI is green at enum-reflection:6227a07, paste `campaign brief`'s
   Evidence table, set State to "ready for review".

## Blocked on

Nothing. The owner may still choose the one split the layout note describes (see Rulings).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130303-a6714d (pending) |
| A2 | split 261121-130303-02426f (pending); no lines move, so no warnings to answer |
| A3 | 261121-130315-050173 (pending): `ctest -N` at main and at head, diffed |
| A4 | `docs/source-layout.md` at enum-reflection:6227a07: lists `enum.h`, kept whole, with its sections and why |
| A5 | CI at enum-reflection:6227a07 (pushed) |
| A6 | 261121-130318-1c55be (pending): headers at main and at head compared; the quest creates none, so none to check alone |
| A7 | the lead's read |

## Result

No file split. `enum.h` (1,726 lines) is kept whole and `docs/source-layout.md` names why:

1. The contract is a single header (ARCHITECTURE.md "a single header", "One header";
   AGENTS.md "enum.h is the only header here"); SPLITTING.md keeps it single-file unless the
   owner decides otherwise.
2. Lines 33-1195 (1,163) are one table, the variadic map `_ENUM_M1`..`_ENUM_M999` plus the
   999-slot `ENUM_COUNT`; the remaining ~530 lines are one unit under 1,000.
3. `test/mutants.py` edits a copy of `enum.h` alone in a scratch dir (only that dir on `-I`), so
   any piece would break the mutation check; `test/mutants.py` is outside this quest's file set.

Docs written: `docs/source-layout.md`.

## Rulings I made

- No split, by the single-header contract (lead's context: "A library whose contract is a single
  header stays single-file unless the owner decides otherwise"). The owner's alternative, if he
  wants one: move lines 33-1195 (the variadic map) into `enum/map.h`, leaving ~560 lines in
  `enum.h`; it needs `test/mutants.py` to copy the piece too. Not recommended: the table is
  edited only as a whole, and the cut costs the single-header contract.
- No open branch edits `enum.h` (`git branch -a`: only main, pins-wave, this quest; none touch it).
- A6 has no header to check; its job proves the quest adds none instead of running
  `selfcontained.sh` over an empty set.
