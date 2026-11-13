# sync-at-first-commit: journal

State: waiting on jobs 261112-115011-237ee0
Quest: .campaign/xapiand/quests/sync-at-first-commit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Durability review CHANGES, B1 fixed at strata:cb5294b8: a failed merge's cleanup no longer syncs the
directory on the merge thread; the job reports the removed bytes (`merge_job::removed`), publish_merges
adds them to `unlinked_`, the writer's next sync_dir() (the inherited sync while owed) returns them.
Test: index_test "A3, durability review B1" (eager executor, four segments, merge output under ENOSPC,
one sync_directory EIO: first commit fences). Locally: passes with the fix; mutant `merge` (restores the
merge-thread sync) fails it at index_test.cc:495. Note D4, Evidence and review table updated.
Jobs at cb5294b8: reviewer's probe rerun 261112-115005-1dba03 (must exit 0); suite 261112-115011-237ee0;
mutant merge 261112-115015-c66d2c (A3, fails-without). A4 not rerun: the commit path is unchanged
(lead's reruns at a297ba00: 261112-111415-c3939c, 261112-111415-99e66d).
Done: probe 261112-115005-1dba03 exit 0; mutant merge 261112-115015-c66d2c exit 0 (killed). Suite running.
N1-N3 (hunt.md) left out: none falls out of this change.

## Next

1. Read the three jobs; if green, `campaign accept sync-at-first-commit durability --confirm B1=<suite job>` is the lead's; State: ready for review.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | [artifact] strata `docs/design/sync-at-first-commit.md` |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | 261112-111415-c3939c, 261112-111415-99e66d (at a297ba00; earlier 261112-090447-4e4f75 open p50 22.93-23.05 ms -> 0.13-0.18 ms, garbage 0/1000; 261112-090455-1f97df first commit 23.08-23.67 -> 24.99-25.76 ms) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- An open at `to` that falls back still commits at open (D6.8): an explicit rollover commit with its own syncs, not the inherited sync. A2's zero-sync claim covers opens that do not fall back.
- Removals of inherited `manifest-*`/old segments before the first commit pay the inherited sync first (D4 of the note) rather than waiting for a commit; EIO there fences. Read-only handles still reclaim garbage.
- The first commit always orders the log (not only the directory): two records in flight would let a whole new edit follow a torn inherited one, which replay reads as damage.
- faultfs's staged-node crash model is corrected (post-barrier writes never survive without the staged bytes); no existing path writes a staged file, so no existing outcome changes.
- manifest-log.md D6.6 and T4 case 9/10 text updated (now in the file set); faultfs model tests in test/faultfs_test.cc.
- F1: while owed, sync_dir() itself is the inherited sync (log + directory, fences, clears owed), so every caller (drain, ENOSPC cleanups, rollover) is covered by one rule rather than skipping.
- F3: faultfs models chained ordering epochs (not a stated limitation); with one ordering point it draws the same outcomes from the same seeds as before.
- faultfs: a failed sync keeps ordered (staged) bytes; a power-off sync has no effect (the crash decides); a write after an ordering point to any file never survives without that epoch (needed once power-off leaves state to the crash). The second was needed for the `after` mutant to be caught: the old model dropped the cache at the power-off sync, hiding the write-before-sync crash point.
- The dropped-segment state is built by replaying a twin index's durable edit bytes unsynced (identity_seed 7), since faultfs cannot kill a process between write and sync.
- bench/open_sync/mutant.sh deleted (its line is gone); open-sync.md points at 8499e781 for it.
- build.sh builds both A4 sides from this checkout's idle_index.cc (main's lacks `commit`).
- B1: option (a), the lead's: the merge thread never syncs the directory; its removed bytes wait in `unlinked_` for the writer's sync. A failed merge's space now returns at the writer's next directory sync (the ENOSPC paths run one), not at once.
