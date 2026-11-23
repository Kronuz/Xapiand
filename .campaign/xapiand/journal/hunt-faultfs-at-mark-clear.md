# hunt-faultfs-at-mark-clear: journal

State: waiting on jobs 261123-132536-5edf40 261123-132538-2cb7f4 261123-132542-c99f18
Quest: .campaign/xapiand/quests/hunt-faultfs-at-mark-clear.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Test (faultlab:17941d4) and fix (faultlab:5d4df9b) pushed to the quest branch of ../faultlab; waiting on the mutant job, the fix job and build-test.

## Next

1. Paste `campaign brief`'s Evidence table below, fill Result, set State "ready for review".

## Blocked on

<What, and who decides. "Nothing" when nothing.>

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>


Leftover confirmed on faultlab main (42cc324): order_files_and_directory pops its mark when its
fault fires before the effect and no file staged, and leaves the at_mark records written inside
it (a non-own file's sync writing). Jobs: 261123-132536-5edf40 (test-only commit 17941d4, must
fail), 261123-132538-2cb7f4 (fix 5d4df9b), 261123-132542-c99f18 (build-test A1).

## Rulings I made

- The leftover can be read: a second point at the removed one's epoch finds the epoch taken in
  at_mark, so note_write skips the record of the bytes before its own write, and a crash
  (epochs none) cuts to the stale bytes ("old") instead of "mid". Missing outcome, not corrupt
  bytes; the earlier fix (7f3910c) only covered an after-effect sync, a plain sync also clears.
- Fix at the removal: filesystem::drop_last_mark pops the mark and erases at_mark records with
  epoch >= the remaining mark count.
