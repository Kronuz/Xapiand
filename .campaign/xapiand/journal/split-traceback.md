# split-traceback: journal

State: waiting on job 261121-145055-e9ab79 (split_check, fixed version, at a6690d9). Merged main (ci.yml only); pushed. Build-test, A2 (594a3e), A3 and A6 passed at 29f0dd1 (tree identical apart from ci.yml).
Quest: .campaign/xapiand/quests/split-traceback.md (the lead's; never edit it)

## Doing

No split. Note committed at traceback:29f0dd1 (branch quest/xapiand/split-traceback). Jobs at
29f0dd1: build-test (A1) 261121-130417-01d2eb, split (A2) 261121-130417-7d9331 (both submitted
by `campaign verify --for A1`; the split job proves A2), ctest -N diff vs 066853d (A3)
261121-130428-319638, new-header self-containment (A6) 261121-130431-d0c0e2.

## Next

1. When build-test 261121-130417-01d2eb, A3 261121-130845-a8f326 and A2 261121-130848-594a3e pass, paste `campaign brief`'s Evidence table and set State to ready for review.
2. main changed .github/workflows/ci.yml since the branch point; the brief suggests `campaign land split-traceback --merge-main` (the lead's).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130417-01d2eb |
| A2 | 261121-130848-594a3e and 261121-145055-e9ab79 (fixed checker at a6690d9, no source change: no warnings expected): split_check.py from the split-check-sources worktree (ignores non-C/C++ files). The landed tool's split check 261121-130417-7d9331 failed only on docs/source-layout.md's prose lines (26 errors, 0 warnings): the pre-split-check-sources tool checks every file; the lead reruns A2 once that tool lands |
| A3 | 261121-130845-a8f326 (ctest -N `Test #`/`Total Tests` lines at base 066853d and head, diffed). Failed 261121-130428-319638: the diff compared unbuilt-executable warnings that name the build dirs ctn-base vs ctn-head, not tests |
| A4 | [artifact] `docs/source-layout.md` at traceback:29f0dd1 |
| A5 | CI at 29f0dd1: all green except freebsd-x86_64, whose vmactions/freebsd-vm step died (ssh exited with code 8), a runner failure, not code; rerun on a6690d9 pending |
| A6 | 261121-130431-d0c0e2 done, exit 0: "headers new since 066853d: none" (selfcontained.sh runs only when a header was added) |
| A7 | the lead's read |

## Result

No split. `traceback.cc` (1,054 lines) stays one translation unit; `docs/source-layout.md` (new)
says why and lists the other files (none over 240 lines).

## Rulings I made

- R1 `traceback.cc` stays whole. Its contract is one source file: Xapiand (CMakeLists.txt,
  `SOURCE_SUBDIR do-not-configure`) compiles `${traceback_SOURCE_DIR}/traceback.cc` alone into
  XAPIAND_OBJ; AGENTS.md calls it "the one translation unit". A split changes that source list
  for every such consumer, and the quest's contract is that nothing users see changes. The
  sections (box ~190, raw layer ~115, throw hooks ~95, dumper ~470 lines) are already bannered
  and gated, and they share file-local internals (the __has_include feature block, the
  anonymous-namespace MAX_DUMP_FRAMES used by backtrace() and the registry, the __cxa_exception
  layout shared by exception_callstack and the __cxa_throw hook); separate TUs need an internal
  header plus namespace changes for a file 5% over the look threshold. If the owner wants it
  anyway, the cut is traceback_dumper.cc (registry + handler + dump/snapshot/registration) and
  traceback_throw.cc (__cxa_exception, exception_callstack, throw hooks), with a Xapiand
  follow-up adding both files at its next repin.
- R2 Platform code is no boundary here: the macOS atos symbolizer is ~75 lines (under the
  100-line floor) beside describe(), its one caller; the per-arch register reads are inside one
  signal handler.
- R3 Open branches: fix/handler-frame-pointer and pins-wave diff empty against main; no Detent
  repository consumes traceback.
- R4 A2 has no moved lines to check; the split job runs over a diff that only adds the note.
