# sync-at-first-commit: journal

State: waiting on jobs 261112-090343-e602b5 261112-090406-e575c5 261112-090414-89aaae 261112-090438-33df17 261112-090447-4e4f75 261112-090455-1f97df
Quest: .campaign/xapiand/quests/sync-at-first-commit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Design review (CHANGES, F1-F6) folded in at strata:34aa3691; implementation of A2-A4 built and
passing locally (index_test, faultfs_test). Jobs queued:
- suite (A2,A3): 261112-090343-e602b5
- mutants (--fails-without, bench/open_sync/first_commit_mutant.sh), done and killed: open A2 261112-090346-f32817
  (index_test.cc:289), drop 261112-090352-1ecddc (:334), after 261112-090358-93f7af (:335); still running:
  drop 261112-090352-1ecddc, after 261112-090358-93f7af, gate 261112-090406-e575c5, dir 261112-090414-89aaae (A3)
- A4: build 261112-090438-33df17; opens+garbage (exclusive) 261112-090447-4e4f75; first commit (exclusive) 261112-090455-1f97df

## Next

1. Read the jobs; put A4 numbers in strata docs/perf/open-sync.md ("Sync at first commit" section) and the note's Cost.
2. `campaign accept sync-at-first-commit design --confirm F1=<suite job> ...` is the lead's; report ready for review.

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
| A4 | <no evidence yet> |

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
- faultfs: a failed sync keeps ordered (staged) bytes; a power-off sync has no effect (the crash decides). The second was needed for the `after` mutant to be caught: the old model dropped the cache at the power-off sync, hiding the write-before-sync crash point.
- The dropped-segment state is built by replaying a twin index's durable edit bytes unsynced (identity_seed 7), since faultfs cannot kill a process between write and sync.
- bench/open_sync/mutant.sh deleted (its line is gone); open-sync.md points at 8499e781 for it.
- build.sh builds both A4 sides from this checkout's idle_index.cc (main's lacks `commit`).
