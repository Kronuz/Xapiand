# strata-journal-behind: journal

State: working
Quest: .campaign/xapiand/quests/strata-journal-behind.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 1 (2026-11-25). Pins bumped to consensus 6d710cb and its wave (task 477a7d3, completion
3061516, journal f66c16e, faultlab 1ac0b17, crc32c 52889db), group_tests building under quiet light.
The check is in group.hh (open reads E after recover/create; adopt refuses P_H > E first).

Lifecycle at open (P_b the recovered bundle, P_H strata's durable head `ix_->applied()`, E
`driver.state(id).last` read on the loop in the continuation of `driver.recover`/`create`, so after
the log's tail is recovered and before any election can append). The check runs first in `adopt`,
before `resume`, the quarantine release and the file set's recover: nothing on disk changes before it.

| Path at open | P_b | P_H | E | Result |
|---|---|---|---|---|
| clean stop (close checkpoints to committed, keeps the closed record) | the close's bundle | = P_b (commit precedes publish) | >= committed >= P_b | opens, nothing replayed |
| crash with tail | last bundle | >= P_b (a commit whose publish was lost) | >= P_H: apply is bounded by the durable commit index (consensus core.h deliver), persisted after its entries | replays (P_H, E] |
| closed record (reopen in process, or after restart) | the close's bundle | >= P_b (in process: applied, past the durable head) | the record's last must match the log's (core.h:323) | resumes its term |
| checkpoint plus tail | the checkpoint's | >= P_b | >= P_H as for a crash | replays (P_H, E] |
| stage complete, consensus persisted the install (resume activates) | boundary B | old index's <= its commit index < B (an install at or below the commit index is refused CaughtUp, core.h:767; a requested reinstall keeps its suffix, so E >= its old last); half-activated: B | >= B (the log's base) | passes; resume activates |
| install activation (activate_image) | B | replaced by the image's B | not read: `adopt(b, true, nullopt)` | no check, on purpose |
| index damaged at open (open_index quarantines, deferred index) | any | 0 | any | never refused; reinstalls |
| journal created (the host holds no group) over an index with state | none | > 0 | the new log's end | refused P_H > E (journal missing) |
| log damaged (bounded) | - | - | - | journal loses the group inside recover, before adopt |
| journal put back older (copy, plain-mode power cut) | <= P_H | > E | older log's end | **refused P_H > E**, nothing written |
| index put back older | > P_H | | | refused P_b > P_H after resume (unchanged) |

A sound group is never refused: every sound row has P_H <= E at the moment E is read.

## Next

1. Tests in test/group/journal_behind_test.cc (clean stop, crash, journal missing refuse byte-equal;
   sound crash replays (P_H, E]); mutant `journal-behind-unchecked` in mutants.py.
2. docs/mechanisms/group-open.md, CHANGELOG.md; verify + mutant jobs; push.

## Blocked on

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

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
