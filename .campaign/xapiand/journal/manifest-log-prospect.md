# manifest-log-prospect: journal

State: working
Quest: .campaign/xapiand/quests/manifest-log-prospect.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Pinned strata at the manifest-log head strata:4d2a541 (CMakeLists.txt). Ported the tests in the
prospect worktree (uncommitted until the first local build is read): reclaim_test, media_test,
directory_test, fixtures_test (a manifest-log.bin golden replaces the retired manifest/live/meta .bin),
format13_test (rewrite via read_state/recount/frame_snapshot), index_test, lifecycle_test,
churn_test, background_merge_test (API shape). Local build against a strata archive of 4d2a541.

## Next

1. Read the local build's errors, fix, run the suite; commit.
2. Submit build-test and the A3 identity job at the head; fill Evidence.

## Blocked on

Strata:4d2a541 is not on origin (origin's branch is at strata:79beaf4), so the `build-test`
check, which fetches the pinned commit from GitHub, cannot run until the lead pushes it or the
check passes `--ref`. Local builds use FETCHCONTENT_SOURCE_DIR_STRATA.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <job id at the current code> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
