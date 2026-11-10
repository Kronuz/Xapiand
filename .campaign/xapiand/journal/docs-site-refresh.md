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
| A1 | `.campaign/xapiand/journal/docs-site-refresh--getting-started.md` and `.campaign/xapiand/journal/docs-site-refresh--architecture.md`: every audit row of the two areas' pages fixed, cited by file:line (D55 scopes A1 to these areas) |
| A2 | 261110-043753-be98ef (smoke, e2e, tls GREEN at the quest head's code) |
| A3 | 261110-071901-6af2c3 (the xapian grep at the head) |
| A4 | 261110-043755-c5fb2d at the quest head: 130 pages, 0 broken links on the two areas' pages; 5 broken pages remain in the reference guide (owed by its task) |
| A5 | 261110-071901-f8e1dd (request count at the head: 425, from 396 at the base, 261110-034139-bc57c3); the features are covered by the getting-started pages' request blocks (task journal), passing in 261110-043753-be98ef |
| A7 | [manual] German read architecture, quick-start and options on 11-10 and approved them ("they all look good"); asked about the WAL and fsync debouncer, which the page describes as shipped (D13: they stay until 7b) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
