# split-strict-stox: journal

State: waiting on jobs 261121-130207-7a60bd 261121-130208-b3f0bb 261121-130225-309d93 261121-130228-8f04e7
Quest: .campaign/xapiand/quests/split-strict-stox.md (the lead's; never edit it)

## Doing

Jobs at strict-stox:5a7e44b: build-test (A1) 261121-130207-7a60bd, split (A2) 261121-130208-b3f0bb,
ctest -N diff vs d3eaf32 (A3) 261121-130225-309d93, new-header self-containment (A6) 261121-130228-8f04e7.

## Next

1. When the jobs pass, paste `campaign brief`'s Evidence table and set State to ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending 261121-130207-7a60bd |
| A2 | pending 261121-130208-b3f0bb |
| A3 | pending 261121-130225-309d93 |
| A4 | `docs/source-layout.md` at strict-stox:5a7e44b |
| A5 | CI on the pushed branch |
| A6 | pending 261121-130228-8f04e7 (no new headers: the job lists headers added since d3eaf32 and runs selfcontained.sh on them if any) |
| A7 | lead's read |

## Result

No split. `strict_stox.hh` (1,510 lines) stays whole; `docs/source-layout.md` (new) says why and
lists the other large files (none over 1,000 lines: tools/verify.cc 995, test/slow_test.cc 862).

## Rulings I made

- R1 `strict_stox.hh` stays one file: the library's contract is a single header (README "One
  header, no dependencies"; "Without CMake, copy `strict_stox.hh` onto your include path"), and
  SPLITTING.md keeps a single-file contract unless the owner decides otherwise. Its layers also
  share macros it defines and `#undef`s at its end. The owner can still choose a split; the
  natural cuts would be the integer fast path, the Eisel-Lemire path with its generated 5^q table,
  the big-integer slow path, and the public `Stox` API.
- R2 Open branches: none edit `strict_stox.hh` (only origin/main; local codex/parser-fuzz is
  merged into main).
- R3 A2 has no moved lines to check; the split job runs over a diff that only adds the note.
