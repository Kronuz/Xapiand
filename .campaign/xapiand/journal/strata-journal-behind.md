# strata-journal-behind: journal

State: ready for review; waiting on jobs 261125-004550-50832e (build-test), 261125-004551-0bc60c (A1 test), 261125-004553-aab292 (A1 mutant)
Quest: .campaign/xapiand/quests/strata-journal-behind.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 1 (2026-11-25). Code at strata:31821753, pushed. Only the consensus pin moves (to 6d710cb);
the rest of its pin wave stays (Blocked on 1). The check is in group.hh (open reads E after
recover/create; adopt refuses P_H > E first). Jobs queued: build-test, the test, the mutant.

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

1. Read the three jobs; on green, State ready for review.

## Blocked on

1. (lead, not blocking this quest) Consensus 6d710cb's own pins (task 477a7d3, completion 3061516,
   journal f66c16e, faultlab 1ac0b17, crc32c 52889db) fail strata's build: completion's PosixIO now
   takes a required FlushMode (`PosixIO(dir, PosixIOOptions{FlushMode, ...}, mode)`), and 15 files
   construct it the old way: bench/{catalog_tiers,eager_open,hybrid,idle_index,kv,kv/probes,kv/proto,
   manifest-log}, test/{counted_hooks.hh,counted_io_test.cc,directory_test.cc}, README.md,
   include/strata/core/index.hh (a comment), test/group/footprint_test.cc. Only the last is in this
   file set. Strata declares task, completion and journal first, so its pins win and consensus
   6d710cb builds and passes against strata's (journal 9c7e31c): group_tests built, the targeted
   group tests pass. Options: (a) land as is (consensus pin only) and move the wave in its own quest
   that picks each bench's flush mode; (b) widen this file set to the 15 paths. Recommend (a): the
   wave is mechanical but separate, and each bench's mode is a measurement choice.

1 answered (lead, 2026-11-25): agreed; only the consensus pin moves here. strata's move to the rest of the pin wave (completion's FlushMode in 15 files) is its own quest, filed in hunt.md.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | test 261125-004551-0bc60c (journal_behind_test: clean-stop, crash, missing, sound); mutant journal-behind-unchecked 261125-004553-aab292 (--fails-without, clean-stop) |
| A2 | build-test 261125-004550-50832e at strata:31821753 |
| A3 | docs/mechanisms/group-open.md (three refusals, what each leaves on disk, the sound paths); CHANGELOG.md 2026-11-25 entry |

Local runs (quiet light, not evidence): journal_behind_test ok (refusals P_H 6/E 3, 5/3, 4/0, every
file byte-equal; sound P_b 3, P_H 3, E 7, 5 documents, seeds 1-8); the mutant fails clean-stop (no
refusal, files changed, the replica then reads damaged); group_journal_behind, lifetime, catalog,
install, repair, file_set, witness: 19 of 19 pass.

## Result

strata:31821753. `group::open()` reads E = `driver.state(id).last` after recover/create and
`adopt(b, restored, log_end)` refuses P_H > E first, `group_fenced` naming both ("strata's durable
head P_H = 6 is past the recovered log's end E = 3: the journal is older than the index"); an
install's activation passes nullopt. How it works: docs/mechanisms/group-open.md. Tests:
test/group/journal_behind_test.cc; mutant journal-behind-unchecked (test/group/mutants.py).

## Rulings I made

- The check runs on create too (recover() serves both): a journal holding no group over an index
  with state is P_H > E with the new log's E (0), the "journal missing" refusal. Before, replay
  skipped new entries at or below P_H.
- Checked before `resume` unconditionally, also when a persisted install stage is about to
  activate: consensus refuses an install at or below its commit index (core.h:767), so the old
  index's P_H < B <= E; a half-activated index has P_H = B. No sound row is refused (table above).
- E is read in open's continuation right after recover/create returns, on the loop, before the
  chore runs recovery: the log's tail is recovered and no election has appended.
- CHANGELOG.md did not exist in strata: created in consensus's format with this entry.
- Pins: consensus only (Blocked on 1).
