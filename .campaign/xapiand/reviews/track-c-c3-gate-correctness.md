# track-c-c3-gate review: correctness

Commit: strata:5cab3e7b21a701c1ce76eeea9aa69c8c412c244e, judging the rows it reports at consensus:b7f48b2414d8b5822ba2924b9a2df1a252e4364a
Verdict: CHANGES
Date: 2026-11-18

Scope: the architect's gate review of Track C's C3 (D10, repair at or below the commit cursor):
whether T5's rows and T2's repair-below-`base_index` mutant prove C3's safety claim, whether the
Core-level proof suffices while the Driver row is owed to `consensus-sim-loops`, and whether the
jobs match `docs/gates/track-c-c3.md`. Code read at consensus:b7f48b2: `core.h` `handle(Repair)`
(637-649), `checkpoint_rejection` (599-612), `deliver` (1220-1230), `log_term` (430); `driver.h`
`repair` (311-317), `Call::begin` (408-418), `admit` (~720-740), the `Reject` and `RepairCompleted`
dispatch (803-808); `worker.h:676`; `consensus_test.cc:1072-1188`; `simulation_test.cc:533-549`;
`driver_test.cc:303-315`; `tools/mutants.sh`. consensus main is now consensus:8876195, which adds
only a ctest timeout to `CMakeLists.txt`, so the rows at b7f48b2 stand for main's code. No GPL
sources were read.

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

I opened each job's spec and read its log (`~/.scratch/campaign/jobs/<id>/a1/log`). The scripts
are `~/.scratch/c3-gate/body.sh` (sha256 prefix e44338e2dcc1, matching the jobs' recorded hash)
and `vm.sh`, which `git archive`s `$C3_COMMIT` into the VM.

| Job | Platform | Result in the log |
|---|---|---|
| 261118-025944-a6e140 | macOS | build at b7f48b2, exit 0 |
| 261118-025951-ea8fde | macOS | `repair: 8 boundaries`, `39611 consensus checks, 0 failures`; `124 driver checks, 0 failures`; simulation `200 seeds x 10000 steps, 1730 elections ... 1081 repairs ...: ok`; driver simulation `20 seeds x 3000 steps ...: ok` |
| 261118-025953-961e8c | macOS | `repair below base_index: unit yes, membership no, simulation yes (400 seeds, first at seed 1)` |
| 261118-025955-8f229e | macOS | `--fails-without`, exit 1: `FAIL: a boundary below base_index is refused` then three cascading failures, `37399 consensus checks, 4 failures` |
| 261118-042015-dba12c | FreeBSD VM 18784 | the same three rows, the same counts, every row exit 0 |
| 261118-042017-9d13e2 | FreeBSD VM 18784 | mutant: unit yes, membership no, simulation yes (first at seed 1) |
| 261118-042020-71911d, 261118-042022-3bd680 | Linux VM 18782 | build exit 0; the same rows and counts, every row exit 0 |
| 261118-042024-031f8b | Linux VM 18782 | mutant: unit yes, membership no, simulation yes (first at seed 1) |

Every macOS number in the report matches its log. The VM jobs finished during this review with
identical counts, as expected of a deterministic suite.

## What I checked myself

Probe 261118-042606-90b1bf (macOS, light, slot `c3-gate-mac` at consensus:b7f48b2, exit 0, 549 s):
`.scratch/review-track-c-c3-gate-correctness/mutants-extra.sh` runs consensus's own
`tools/mutants.sh` with its list replaced by three repair mutants, 400 seeds by 10,000 steps:

| Mutant (`core.h` `handle(Repair)`) | unit | membership | simulation |
|---|---|---|---|
| P1: the term check at the base boundary dropped (`b.index == base_index_ ? false : ...`) | no | no | no |
| P2: the commit bound dropped (`b.index > durable_.commit_index` removed) | yes: "a boundary above applied is refused" and 8 cascading failures | no | no |
| P3: a repair replays from one past its boundary (`applied_ = b.index + (b.index < commit)`) | yes | no | yes: `seed 1: applied sequences diverge` |

### Question 1: do T5's rows and the mutant prove C3's safety claim?

Mostly, and the proof is mostly structural. `handle(Repair)` writes only `applied_` and `epoch_`,
then `deliver`s from the retained log (`core.h:637-649`). It touches neither the log nor
`base_index_`, so a repair cannot rewrite an entry or truncate. `base_index_` moves only through a
checkpoint, and `checkpoint_rejection` refuses `through <= base_index_`, `through > applied_` and
any epoch but the current one (`core.h:601-602`). So a capture of damaged state never becomes the
base, and a checkpoint taken after a rewind cannot pass the rewound cursor.

What the tests show:

1. A replay that delivers the wrong entries is caught by both suites. P3 fails the unit loop's
   comparison with the undamaged replica and T2's "applied sequences diverge" oracle at seed 1, so
   the oracle has teeth on repairs, not only on replication.
2. The below-`base_index` mutant is killed, but the simulation kills it through a fence, not an
   oracle: `seed 1: node 1 fenced: log index outside retained suffix` (the slot's
   `build/mutants/repair-below-base_index/simulation.log`). Without the check, `log_term` throws
   on an index below the base and the node fences. So the guard turns a fence into a clean
   `InvalidRepair`; it is not what keeps the log intact. The unit row asserts the clean refusal.
3. The boundary-term validation has an untested branch (F1). The "term differs" row probes index
   7 while the base is 0, so the base branch `b.term != base_term_` is never exercised: P1
   survives all three suites. That branch is C3's only check that an image at the base belongs to
   this log.
4. The commit bound is killed only by accident (F2). The "above applied" row repairs at
   `applied + 1`, which is past the log's end (there `applied == commit == last_index`), so
   without the bound `log_term` throws and the core fences. No row repairs at a boundary in
   `(commit, last_index]` with a matching term, and T2 draws `b` from `[0, applied]` only
   (`simulation_test.cc:536`).

### Question 2: is C3 sound at the Core level alone?

Yes for safety. `Driver::repair` posts `Repair{request, boundary}` (`driver.h:314`). The Driver
maps a `Reject` to `rejected(reason)` (`driver.h:803-804`) and a `RepairCompleted` to the waiter,
or to the Sink when the waiter is gone (`driver.h:807-808`). A Worker `Busy` (delivery or
checkpoint in flight, `worker.h:676`) makes `admit` stop and retry on a later step. No Driver path
changes the log, the base, the applied cursor or the epoch, so every safety property is Core's (T5,
T2) plus the Worker's checkpoint-in-flight refusal (`worker_test.cc:941-951`). The Driver row
(`driver_test.cc:303-315`, a lone voter repairing at its applied boundary in epoch 1) proves the
forward and the completion. The Driver-level rows that remain cover the integration and the
caller's contract, not safety, and `consensus-sim-loops` A3 owns them. The gate need not wait
for them (F3 records the quest-text mismatch).

### Question 3: do the jobs match the report?

The macOS numbers all match (table above). The Linux and FreeBSD rows were still queued in the
report, and they have finished since with the same counts. A1 is still pending in
`campaign status`, because the jobs ran in `consensus` while the quest's repository is strata. The
lead credits them by hand. Only A1's three-platform claim depended on the VM jobs; the safety
conclusions above did not.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | T5's row "a boundary of another term is rejected" is proven only above the base. The base branch of the term check (`b.index == base_index_ ? b.term != base_term_`) has no killing test: with it dropped, a repair at `base_index` under any term is accepted and redelivers on top of a foreign image. That branch is C3's only check that an image at the base belongs to this log. | Probe 261118-042606-90b1bf, P1: unit no, membership no, simulation no; `consensus_test.cc:1100` probes the term only at `applied` while the base is 0 | In consensus `repair_at_or_below_commit`, after the base is raised (`consensus_test.cc:1111`), add `Repair{..., {through, term_at(through) + 1}}` expecting `InvalidRepair` ("a boundary at base_index with another term is refused"). Add P1 to `tools/mutants.sh`. Run a `--fails-without` job at the mutant and add its row to the gate report. |
| F2 | no | gap | The commit bound (`b.index > durable_.commit_index`) is killed only because the "above applied" row repairs past the log's end, and `log_term` throws, which fences the core. No row repairs at a boundary in `(commit, last_index]`, a follower holding persisted entries it has not seen committed, and T2 never draws a boundary above `applied`. The row's name also misstates the rule: the core accepts `(applied, commit]` by design (consensus:ec95656) and refuses above `commit`. | Probe P2: the unit suite fails with 9 failures in cascade, the simulation does not fail | Add a row in which a follower holds an appended, uncommitted entry and is refused `InvalidRepair` at it. Rename "above applied" to "above commit" in the test and the gate report. |
| F3 | no | gap | The quest outcome and the Reconciliation table say "through `Driver::repair`", but T5's rows and the mutant run at `Core::step`. The Driver row is one lone-voter repair, and no Driver-level row rejects a repair below the base. The report states this honestly; `consensus-sim-loops` A3 owns the remainder. | `docs/gates/track-c-c3.md`, "Reading the rows"; `consensus-sim-loops.md` A3 | The lead records the deferral in the quest's Decisions, so A1 is not read as met through the Driver. |
| F4 | no | gap | Liveness under load is unmeasured. A repair is admitted only with no delivery outstanding (`core.h:640`, `worker.h:676`), and `handle(Applied)` delivers the next batch in the same step. On a group that commits continuously, a repair may wait a long time. That is not a safety issue. | Code reading | `consensus-sim-loops` A3 reports the repairs requested and completed under load, and how long each waited. |
| F5 | no | nit | The report's cells are stale: FreeBSD's Driver row (dba12c) and every Linux row and mutant (71911d, 3bd680, 031f8b, 9d13e2) are done and green. A rows log on a VM shows `up on <port>` but no `uname` or commit line; only the build jobs print them. | The job logs above | Fill the cells. Optionally print `uname -srm` and the commit in `body.sh rows`. |
| F6 | no | nit | `handle(Repair)`'s ternary duplicates `log_term`, which already returns `base_term_` at the base (`core.h:430`): one way to check a boundary would be `log_term(b.index) != b.term`. | `core.h:430`, `core.h:642` | Optional, in consensus. It does not remove the need for F1's row. |

## Not run

- Sanitizers and CI for consensus at b7f48b2: CI's.
- No Driver-level repair probe. The Driver simulation issues none; that is `consensus-sim-loops`
  A3 (F3, F4).
- The host-side parts of D10 (verifying a fetched image whole, the snapshot fetch from a member,
  and withholding `nearest` and `session` reads until `applied` recovers) are outside these rows,
  and I did not examine them.
- I did not read strata's own repair host code; the gate is about `consensus`.
- The build-test check 261118-042103-6cde19 on the strata branch was still queued.
