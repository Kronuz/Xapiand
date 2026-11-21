# split-traceback: journal

State: waiting on jobs 261121-130417-01d2eb 261121-130417-7d9331 261121-130428-319638 261121-130431-d0c0e2
Quest: .campaign/xapiand/quests/split-traceback.md (the lead's; never edit it)

## Doing

No split. Note committed at traceback:29f0dd1 (branch quest/xapiand/split-traceback). Jobs at
29f0dd1: build-test (A1) 261121-130417-01d2eb, split (A2) 261121-130417-7d9331 (both submitted
by `campaign verify --for A1`; the split job proves A2), ctest -N diff vs 066853d (A3)
261121-130428-319638, new-header self-containment (A6) 261121-130431-d0c0e2.

## Next

1. When the jobs pass, paste `campaign brief`'s Evidence table and set State to ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending build-test 261121-130417-01d2eb |
| A2 | pending split 261121-130417-7d9331 (no moved lines: the diff only adds the note) |
| A3 | pending 261121-130428-319638 (ctest -N at base 066853d and head, diffed) |
| A4 | [artifact] `docs/source-layout.md` at traceback:29f0dd1 |
| A5 | CI on the pushed branch |
| A6 | pending 261121-130431-d0c0e2 (no new headers: the job lists headers added since 066853d and runs selfcontained.sh on them if any) |
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
