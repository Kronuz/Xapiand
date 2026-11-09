# wbuf-inline-prospect: journal

State: ready for review
Quest: .campaign/xapiand/quests/wbuf-inline-prospect.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done.

## Next

1. Lead: rebase onto prospect main (ci.yml changed), re-verify, land after wbuf-inline.

## Blocked on

Nothing

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261109-105845-6fd04e (ctest 51 of 51 incl. the 16,000-record budget case) and 261109-105846-df7232 (build-test) |
| A2 | 261109-105846-df7232 (build-test at prospect:63e11d3, strata pin 3df7af4ff4ac) |
| A3 | 261109-105845-6fd04e: ctest 51/51; churn plain 132 lines, longkey 2 lines, dump 36 lines identical to prospect on strata 9d52824 at flush threads 1 and 2 |

## Result

Repinned strata 9d52824 -> 3df7af4ff4ac in CMakeLists.txt (prospect:63e11d3); budget test (prospect:c0c4260) unchanged. No behavior difference vs earlier pin in churn/dump. No docs needed.

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
