# split-docset: journal

State: waiting on job 261121-145049-f8836d (A2, fixed checker at docset:3df5985, main merged; A1/A3/A6 reused, passed at 2878b0e, merge touched only ci.yml)
Quest: .campaign/xapiand/quests/split-docset.md (the lead's; never edit it)

## Doing

Jobs at docset:2878b0e (pushed): build-test (A1) 261121-130438-3642f5; split (A2)
261121-130439-48037a failed: the tool the check runs today treats docs/source-layout.md as source
(23 errors, all on the note's lines), which split-check-sources fixes; the lead reruns A2 once it
lands. ctest -N diff (A3): 261121-130440-16e9c6 failed on my command (ctest -N on an unbuilt tree
prints executable search paths that name each build dir), resubmitted as 261121-130855-ced313
keeping only the `Test #` and `Total Tests` lines. A6 261121-130443-a0ab12 passed: no new headers.

## Next

1. When 261121-145049-f8836d passes (replaces failed 261121-130439-48037a for A2), set State to ready for review; answer any warning.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-130438-3642f5 passed (reused at 3df5985) |
| A2 | 261121-145049-f8836d (split-check-sources checker at 3df5985; pending); replaces 261121-130439-48037a, which failed (pre-split-check-sources tool reads the .md note as source: 23 errors, all docs/source-layout.md lines); local run of split_check.py (split-check-sources worktree) at docset:2878b0e vs 53da120: 0 errors, 0 warnings, 2 notes (docset.hh 1,455 and test/docset_old.hh 1,171 over 1,000, both kept whole) |
| A3 | 261121-130855-ced313 passed (reused at 3df5985; 261121-130440-16e9c6 superseded) |
| A4 | [artifact] `docs/source-layout.md` at docset:2878b0e |
| A5 | CI on the pushed branch at 3df5985 |
| A6 | 261121-130443-a0ab12 passed (no new headers: the job lists headers added since 53da120 and runs selfcontained.sh on them if any) |
| A7 | the lead's read |

## Result

No split. `docset.hh` (1,455 lines) stays whole; `docs/source-layout.md` (new, docset:2878b0e)
says why, names the layers that would be the cuts if the owner chooses a split, and lists the
other files (none hand-written over 1,000; `test/docset_old.hh` is the exempt frozen reference).
No code changed, so no hot-path size comparison applies.

## Rulings I made

- R1 `docset.hh` stays one file: the library's contract is a single header (README "One header,
  no dependencies"; "Without CMake, copy `docset.hh` onto your include path"), and SPLITTING.md
  keeps a single-file contract unless the owner decides otherwise. Same ruling as split-strict-stox
  R1 and the lead's on split-uinteger-t. The owner can still choose a split: the cuts would be
  flat sets (iterator, bitset, sorted_set), 16-bit array kernels, bitmap words and extraction,
  container and set, frozen form; each a contiguous slice, same token stream per TU.
- R2 Open branches: explore/bitmap-and edits docset.hh (+7, a code_align prototype from
  2026-11-02); its note already landed on main (docset:f9d31e9) and explore merges are docs only,
  so it does not hold the file. Moot anyway with no split.
- R3 A2 has no moved lines to check; the diff adds only the layout note.
