# logger-workpool-repin: journal

State: waiting on jobs 261116-080905-f910ab 261116-080906-dbeaea (A1, A2) 261116-080951-7fad0e (A3)
Quest: .campaign/xapiand/quests/logger-workpool-repin.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Repin committed and pushed; jobs running.

## Next

1. When jobs pass: read A3 report (ab/aa report.json), fill Result; check CI.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | build-test 261116-080905-f910ab |
| A2 | e2e 261116-080906-dbeaea |
| A3 | 261116-080951-7fad0e (Detent's A2 job 261116-051232-7ae5c2 ran logger's own workpool pin 640c66b, not Xapiand's 406d2f3, so not cited) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- Renamed src/xapiand_threadpool_trace.h to xapiand_workpool_trace.h and its macros to WORKPOOL_THREAD_REGISTER/UNREGISTER (workpool's names; the old ones would silently drop crash-registry hooks); docs/dev/extraction-backlog.md follows. Outside CMakeLists.txt but required by the rename.
- A3 script .campaign/xapiand/ab-logger-workpool-repin.sh builds both logger commits against workpool 406d2f3 (FETCHCONTENT_SOURCE_DIR_WORKPOOL), 5 rounds A/B then A/A.
