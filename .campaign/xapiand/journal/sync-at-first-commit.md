# sync-at-first-commit: journal

State: waiting on jobs 261112-110230-d89b3f 261112-110231-eee06e 261112-110234-122749 261112-110237-c67ad9 261112-110241-adb8c7 261112-110244-c9702e
Quest: .campaign/xapiand/quests/sync-at-first-commit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Suite 261112-090343-e602b5 at 34aa3691 failed 7 of 75 (reclaim, reclaim_bg_crash, lifecycle(+mapped),
reclaim_mapped, reclaim_bg_crash_mapped, directory). Cause, by local bisection on reclaim_test:
- new index.hh + main faultfs: ok; main index.hh + new faultfs: FAILS (16 failed opens) -> the model, not the code;
- new faultfs without only the power-off change: ok -> that change exposed a model gap: a write after an
  ordering point to a file the point did not stage (a commit's log edit after its segments' ordering) could
  survive without the staged epoch. The IO contract forbids that state.
Fix at strata:9e120989: faultfs snapshots a file's bytes at the first write after each ordering point; a crash
that loses that epoch keeps those bytes, not later ones (new faultfs_test case). Locally all of reclaim,
reclaim bg, lifecycle, directory, faultfs, index pass.

Jobs at 9e120989: suite 261112-110230-d89b3f; mutants open 261112-110231-eee06e (A2), drop 261112-110234-122749,
after 261112-110237-c67ad9, gate 261112-110241-adb8c7, dir 261112-110244-c9702e (A3).
Earlier at 34aa3691: mutants open f32817, drop 1ecddc, after 93f7af, gate e575c5, dir 89aaae all killed.
A4 done (34aa3691; code since changed only faultfs/tests): opens 261112-090447-4e4f75, first commit
261112-090455-1f97df, build 261112-090438-33df17; numbers in strata docs/perf/open-sync.md.

## Next

1. Read the suite and mutant jobs; if green, State: ready for review.

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
| A4 | 261112-090447-4e4f75 (open p50 22.93-23.05 ms -> 0.13-0.18 ms, garbage 0/1000), 261112-090455-1f97df (first commit p50 23.08-23.67 -> 24.99-25.76 ms, +2.0 ms) |

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
