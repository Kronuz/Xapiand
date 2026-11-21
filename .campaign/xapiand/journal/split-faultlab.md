# split-faultlab: journal

State: waiting on jobs 261121-130439-4b9352 261121-130440-2801d9 261121-130441-7cc706 261121-130444-b53329 261121-130230-a50d5a 261121-130448-3a21ae
Quest: .campaign/xapiand/quests/split-faultlab.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Split committed (faultlab:2f5fa62) and layout note (faultlab:71a617f); branch pushed at
faultlab:71a617f. Only jobs remain.

## Next

1. Read the jobs: A1 build-test, A2 split (landed tool and the split-check-sources tool), A3 base
   vs head `ctest -N` logs diffed, A6 selfcontained over the four new headers.
2. Answer split_check warnings below; set State to "ready for review".

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130439-4b9352 at faultlab:71a617f (pending) |
| A2 | split 261121-130440-2801d9 (landed tool); 261121-130441-7cc706 (split-check-sources tool) (pending) |
| A3 | base faultlab:e3fcfe3 261121-130230-a50d5a; head faultlab:71a617f 261121-130444-b53329 (pending) |
| A4 | `docs/source-layout.md` lists the four pieces, what each holds, and that no file over 1,000 lines remains |
| A5 | CI on the pushed branch |
| A6 | selfcontained.sh -s 20 over faultfs/{types,filesystem,file,io}.h: 261121-130448-3a21ae (pending) |
| A7 | the lead's read |

## Result

`include/faultlab/faultfs.h` 1,480 lines -> 46-line umbrella (the model's description, then the
four includes) plus `include/faultlab/faultfs/`: `types.h` 215 (options, `detail::inode`, path
helpers), `filesystem.h` 767 (class filesystem and its detail access structs), `file.h` 286
(class file), `io.h` 264 (class io). Layout note: `docs/source-layout.md`. No code changed.

## Rulings I made

- Open branches: every faultlab branch (Detent's faultfs-collect-linear, faultlab-core, -netlab,
  -strata-gaps, -disklab, -journal-oracle) is 0 commits ahead of main and their worktrees are
  clean, so no open edit of faultfs.h.
- Cut at the four class boundaries: each piece is one thing a maintainer opens alone (the
  options and per-file byte state; the model and crash rebuild; the File handle; the IO
  directory). file and io stay separate: two interfaces, each over 250 lines.
- filesystem.h stays one piece (767 lines): one class body, never cut. `detail::crash_access`,
  `model_access` and `inspection` (lines 949-987) stay with it, in order, since the access
  structs reach its privates and `inspection` sits between it and its users.
- `FAULTLAB_CHECK_COUNTS` (lines 45-48) moves into filesystem.h, the only piece that reads it;
  tests define it before including faultfs.h, so behavior is unchanged.
- The model's description (lines 4-40) stays in the umbrella: it describes all four pieces.
- Each piece starts with the repository's SPDX and copyright lines, then the comment saying what
  it holds (the repository convention puts the license first).
- Added includes per piece: what each uses; `<cstddef>` and `<iterator>` are new (ptrdiff_t,
  std::distance and std::begin were transitive before). Every original include reappears.
- No install rules or package include lists exist (INTERFACE library), so CMake is unchanged.
- No shared helper changed linkage; no `static` dropped; no dynamic initializer moved
  (`filesystem::power_off_` is a static inline member inside the class).
