# sync-at-first-commit: journal

State: design ready
Quest: .campaign/xapiand/quests/sync-at-first-commit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Design ready: strata docs/design/sync-at-first-commit.md (A1). Waiting for the lead's durability design review before building A2-A4.

## Next

1. After the review: index.hh (open drops its sync; inherited-owed flag and segment mark; append path orders segments + log always while owed; rollover clears; retain() gates inherited manifest/segment removals on sync_files_and_directory({log}), fencing on failure).
2. faultfs.hh: staged-node crash model (no post-barrier bytes without staged ones) and a syncs() counter.
3. index_test: A2 sync count across opens; A3 crash enumeration (cases 9/10 x append-no-file/append-segment/rollover; reclaim gate case); mutants as --fails-without jobs via a committed script.
4. A4: idle_index `commit` option; loaded.sh open and first-commit A/B, exclusive <=10 min each.

## Blocked on

The design review (lead launches it).

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
- Out of file set, for the lead: manifest-log.md D6.6 text, and a direct model test in test/faultfs_test.cc (fallback: index_test.cc).
