# split-check-members: split_check proves member definitions moved out of a class body; SPLITTING.md restructures giant classes

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status split-check-members`.

## Outcome

Two classes hold most of their files: strata's `basic_index` (5,309 of index.hh's 6,139 lines) and
`uinteger_t` (5,709 of uinteger_t.hh's 6,042). SPLITTING.md forbids cutting a class body, so both
were kept whole. The owner, 2026-11-21: "One class body of 5K+ lines is just not right! Not right at
all. It makes this single file one of the busiest ones and it also makes the file very very hard
to understand", and for uinteger_t, "algorithms should be split into multiple files". Quests
index-restructure and uinteger-restructure move member definitions out of each class body into one
header per mechanism or algorithm family. This quest gives them a mechanical proof that the move
changed no code, and puts the rule in SPLITTING.md.

## Acceptance criteria

- A1 `split_check.py --members` (or an equivalent mode) proves an out-of-class move: every member
  function body removed from a class body reappears as an out-of-class definition in a piece, with
  its body unchanged except a uniform change of leading indentation, and its declaration left in
  the class. Allowed signature changes are exactly: in-class-only specifiers dropped from the
  definition (`static`, `virtual`, `override`, `final`, `explicit`, `friend` stays in the class,
  default arguments), `inline` added, a `template <...>` header and the class qualifier (`C::`,
  `C<P...>::`) added, and a trailing `;` on the declaration. Anything else fails, naming the member:
  the `test` check, with fixtures for a class template (`basic_index<Layout>`'s shape), a
  non-template class (`uinteger_t`'s shape), constructors with initializer lists, operators,
  `const`/`noexcept`/ref-qualified members, trailing return types, nested classes, and overloads
  with the same name
- A2 A mutated move fails with the member named: one fixture each for a changed statement, a
  reordered statement, a dropped definition, a changed default argument and a changed signature
- A3 The existing modes behave as before: every existing fixture passes unchanged, and the command
  line and exit codes stay as they are (22 gate checks call it): the `test` check
- A4 [artifact] `docs/SPLITTING.md` gains a section on restructuring a large class, holding the
  owner's words above: a class body holding several mechanisms or algorithm families is not "one
  coherent unit"; it is restructured on its own quest (step 1, out-of-class definitions by
  mechanism, proved by A1's mode and a function-size diff; step 2, subobjects where state stands
  apart, reviewed); "Keep whole" names only a class whose members only make sense together, and
  never one because a class body can't be cut. "What `tools/split_check.py` checks" and
  `tools/README.md` describe the new mode
- A5 ruff check and ruff format pass, and CI is green: the `lint` check; [ci]

## Contracts

`split_check.py`'s existing command line and exit codes stay as they are. The new mode is opt-in.

## Decisions

- Lead, 2026-11-21: one shared checker for both restructures (index-restructure,
  uinteger-restructure), not a mover script: contributors move code by hand or with throwaway
  scripts, and this mode is the independent proof. Low risk: tools and docs. It needs
  split-check-anchor, which holds split_check.py now.

## May land early

Nothing: one landing.
