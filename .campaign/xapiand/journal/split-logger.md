# split-logger: journal

State: waiting on jobs 261121-122612-10374d 261121-122612-125a98 261121-122646-d3f2c2 261121-122630-7a4f4d
Quest: .campaign/xapiand/quests/split-logger.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 1 done at logger:992f986 (branch quest/xapiand/split-logger, pushed). Two splits; the rest kept whole.

## Next

1. When the jobs finish: fill the Evidence table from `campaign brief`; report to the lead.

## Blocked on

Nothing. campaign check names no overlap with logger.cc / logger.h; the repository's detent/xapiand branches (logger-burst-cost, -exit-crash, -followups, -on-wheel, -slack-mutant, -wheel-slim, logger-exit-lifetime, logger-fork-reset) all diff empty against main for those two files. The explore branch logger-r2 adds 2 lines to logger.cc (a docs-only exploration, never merged as code).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-122612-10374d (build-test) |
| A2 | job 261121-122612-125a98 (split). Local run of tools/split_check.py at 992f986 (cpp-libraries split-check branch dc0ecfe): 0 errors, 0 warnings, 1 note (logger.cc over 1,000, kept whole) |
| A3 | job 261121-122646-d3f2c2: ctest -N at base 18be8a3 and head in one job, diffed (a quest job supersedes a queued one, so the separate base job was cancelled; also 261121-122628-68f755 at head) |
| A4 | docs/source-layout.md in the logger repo |
| A5 | CI on the pushed branch |
| A6 | job 261121-122630-7a4f4d: tools/selfcontained.sh -x logger.h . (logger_format.h is the only new header) |
| A7 | the lead's reading |

## Result

- logger.h 1,037 -> 774 lines: logger_format.h (303 lines) holds ScratchSlot, the fast formatter and FormattedMsg (two slices of the old header: the scratch block and the formatter). logger.h includes it, so no user changes an include.
- logger.cc 2,211 -> 2,050 lines: logger_timestamp.cc (195 lines) holds default_timestamp, its grey ramp, put_digits and local_tm. Added to the logger target in CMakeLists.txt.
- Kept whole (docs/source-layout.md, with reasons): logger.cc stays one TU (LogWorker is one 550-line class in a TU-local namespace that Logging's members reach; the exit-order statics Logging::config/hooks/handlers + Teardown need one TU; sinks/render (~140 lines) and clocks (~115 lines) share helpers with the worker and Logging, so cutting them costs an internal header and external linkage for little). logger.h keeps the queue records, Logging, Log and macros together (one inline contract).
- No code moved across translation units except default_timestamp (its helpers are used by nothing else), so no linkage change, no dropped static, no dynamic-initializer object moved.

For the lead: docs/exit-lifetime.md line 80 says `scratch_slot` is in logger.h; it is now in logger_format.h (included by logger.h). That file is outside this quest's file set; a one-word edit.

## Rulings I made

- Did not split the worker, sinks or clocks out of logger.cc: SPLITTING.md forbids cutting a class body or a TU-local namespace, and the pieces left would need an internal header plus linkage changes for ~100-140 lines each.
- Two commits, one per split file (logger.h, then logger.cc with CMake and the layout note).
