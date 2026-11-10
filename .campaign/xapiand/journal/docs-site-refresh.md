# docs-site-refresh: journal

State: working
Quest: .campaign/xapiand/quests/docs-site-refresh.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | getting-started and architecture areas: every audit row of their pages fixed, cited by file:line in the task journals (docs-site-refresh--getting-started.md, docs-site-refresh--architecture.md); reference-guide and tutorials areas owed |
| A2 | e2e 261110-043753-be98ef at the quest head (smoke, e2e, tls GREEN; 425 requests, 261110-043757-b6665c, from 396 at the base, 261110-034139-bc57c3) |
| A3 | 261110-034548-e17d04 (0 matches on the architecture pages); the getting-started grep over its 13 pages printed nothing (task journal) |
| A4 | 261110-043755-c5fb2d at the quest head: 130 pages, 0 broken links on the two areas' pages; 5 broken pages remain in the reference guide (owed by its task) |
| A5 | getting-started area: COMMIT, CHECK, UPSERT, PATCH, the options and the response shape documented with request blocks (task journal); the rest owed |
| A6 | owed (docs/dev/docs-coverage.md, with the last task) |
| A7 | [manual] German read architecture, quick-start and options on 11-10 and approved them ("they all look good"); asked about the WAL and fsync debouncer, which the page describes as shipped (D13: they stay until 7b) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
