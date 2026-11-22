# split-traceback: journal

State: waiting on job 261121-145055-e9ab79 (split_check, fixed version, at a6690d9). Merged main (ci.yml only); pushed. Build-test, A2 (594a3e), A3 and A6 passed at 29f0dd1 (tree identical apart from ci.yml).
Quest: .campaign/xapiand/quests/split-traceback.md (the lead's; never edit it)

## Doing

No split. Note committed at traceback:29f0dd1 (branch quest/xapiand/split-traceback). Jobs at
29f0dd1: build-test (A1) 261121-182200-d7fe5b, split (A2) 261121-130417-7d9331 (both submitted
by `campaign verify --for A1`; the split job proves A2), ctest -N diff vs 066853d (A3)
261121-130428-319638, new-header self-containment (A6) 261121-130431-d0c0e2.

## Next

1. When build-test 261121-182200-d7fe5b, A3 261121-130845-a8f326 and A2 261121-130848-594a3e pass, paste `campaign brief`'s Evidence table and set State to ready for review.
2. main changed .github/workflows/ci.yml since the branch point; the brief suggests `campaign land split-traceback --merge-main` (the lead's).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-182200-d7fe5b |
| A2 | 261121-155809-4e674f: the split check (landed split_check.py) at a6690d9, passed |
| A3 | 261121-130845-a8f326 (ctest -N `Test #`/`Total Tests` lines at base 066853d and head, diffed). Failed 261121-130428-319638: the diff compared unbuilt-executable warnings that name the build dirs ctn-base vs ctn-head, not tests |
| A4 | [artifact] `docs/source-layout.md` at traceback:29f0dd1 |
| A5 | CI at a6690d9 (run 44958843249): every job green except freebsd-x86_64, where traceback_hooks aborts in its subprocess. traceback main has failed the same way since 2026-11-19 (run 44570136588, at 47c5736), so the cause predates the split; the lead lands with --no-ci and files the bug in hunt.md |
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
