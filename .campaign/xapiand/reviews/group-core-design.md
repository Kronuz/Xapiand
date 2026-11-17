# group-core review: design

Commit: strata:491a1bdc2aa1e8856b9d7a583e90c4fabf673998
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Reviewed before any code: `quest/xapiand/group-core` is at strata:491a1bdc with nothing built; the
design is Xapiand:0900fc828c8b `docs/design/cluster-release.md` (D3, D4, D11, D16; X10), and the
quest file is at Xapiand:bf93ad540a58.

The shape is right: a lone voter at `rf = 1`, apply off the loop, quorum-probe reads, a bundle that
names strata's manifest and an application file set, close after a checkpoint. A contributor
following the text would still make two expensive mistakes: a recovery rule that loses blob bytes
at a reachable crash point (B1-B3, bytes and a durability mechanism), and acknowledgements at
commit for commands apply can refuse (B4, the API's semantics). Every fix is text plus a named
test; none changes the approach.

The six questions:

1. Checkpoint and recovery: unsafe as worded (B1-B3). With B1's rule it is safe at every crash point
   of the model, a crash inside recovery included, and it composes with document-gate's turn log
   (the same rule: open at the head, skip at or below it, `standalone-turn-log.md` D6) and with the
   supersede guard (consensus's checkpoint carries the retained suffix, `checkpoint.h`); the close
   path bypasses that guard (N1).
2. Event order: `ProposalPlaced` for `none` and commit for `quorum` are right for durability; what
   the reply says is not settled (B4). A3 needs held followers (N3).
3. `read_applied` after the probe under deposition: sound on Core as built (N4).
4. Close after a covering checkpoint: sound; A5's observable and mutant need restating (N1).
5. Idle cost at 10^4: A6 as written cannot pass (B5).
6. Criteria: A1's crash model misses the file set's power loss (B6); A7 depends wholly on
   `detent:consensus-quiescence` (N2); X10 has no criterion (N7).

## Evidence I relied on

No job establishes a criterion at 491a1bdc (`campaign status group-core`: A1-A9 without evidence).
I read the contracts the quest consumes, at their pins: consensus:57745e8 (`docs/DRIVER.md`,
`driver.h`, `worker.h`, `core.h`, `checkpoint.h`); journal `main` 7c08540 and
`quest/xapiand/turn-seal` b08212f (`host.h`, `turn_log.h`); strata 491a1bdc (`index.hh`,
`catalog.hh`, `faultfs.hh`, `docs/perf/small-indexes.md`, `docs/REPLICATION.md`); strata:8e919415
`docs/design/standalone-turn-log.md`; Detent's `consensus-quiescence` quest file. Clean room: no
Xapian or other GPL source read.

## What I checked myself

Scripts under `.scratch/review-group-core-design/` in the home checkout, to rerun on the fix:

- Job 261117-022356-9bafce, `bundle_model.py`: a lone voter's checkpoint bundle with a file set
  (volumes, an aborted upload, a staging file, an upload open across a checkpoint), a crash after
  every atomic step under power-loss and process-crash semantics, recovery, replay to the log's end,
  and the digest (documents resolved through their locators, live files, the volume counter) against
  an uncrashed run of the same log. Six variants; exit 0 means `fixed` and `sync-late` fail nowhere
  and every other variant fails somewhere.
- Job 261117-022605-b29cff, `verdict_model.py`: every schedule of two writes to one id, a commit, an
  apply and an election, with `ack=quorum` at commit and apply refusing a version mismatch, under
  three prepare rules.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B1 | yes | defect | Recovery has two positions and the design names one. The bundle's file set is at the checkpoint's position P_b (the consensus base index); strata reopens at its durable head P_H, ahead of P_b after a crash between `commit_files` and the checkpoint record's barrier, after a capture the Core rejects or a cancelled `checkpoint` (DRIVER.md, "What completes when"; `driver.h:851`), and after any strata commit outside the group (the catalog's default commit hook, `catalog.hh:368`). A1's "truncate to the bundle, delete what it does not name, replay from the manifest's applied position" then drops every blob written in (P_b, P_H] while strata keeps the documents that point at it. REPLICATION.md's reading ("the applied position recorded in the referenced manifest") fails instead: strata keeps no older generation (`retained_predecessors = 0`, `index.hh:525`) and its open fails closed on a missing file (`index.hh:677-689`; retention, `:5134-5146`), which leaves a lone voter unrecoverable | Job 261117-022356-9bafce: `a1-literal` fails 74 of 272 crash points, process crashes included (document `a` reads `MISSING vol-0[0:4]` after a crash between strata's commit and the bundle); `fixed` fails 0 of 544, a second crash inside recovery included | State in D16, A1 and the mechanism doc: (1) strata opens at its durable head P_H, and recovery fails closed if P_b > P_H (document-gate D6's cross-check); (2) the group delivers every committed entry above P_b: file effects always re-run, and strata's apply skips at or below P_H (`index.hh:803-806`); (3) P_b, the file lengths and the application state are taken on the apply chore between two entries; (4) files are synced before the bundle publishes, which is the binding order (`sync-late`, syncing after strata's commit, fails 0); (5) no read is served until applied reaches P_H. A1 names both windows as crash points, and a mutant that replays the file set from P_H fails it (`--fails-without`) |
| B2 | yes | gap | Files the latest bundle names are deleted before a newer bundle publishes: D12's final entry "reads and deletes" its staging file, and an entry of a higher term deletes an open upload's volume. A crash before the next checkpoint leaves recovery a named file that is gone, with the chunks that wrote it at or below P_b and compacted | Job 261117-022356-9bafce: `eager-delete` fails 11 crash points, process crashes included ("named file stage-x is gone") | A rule in the file-set contract: the application retires a file, and the group unlinks it only after a bundle that omits it publishes, then syncs the directory. A1 adds the staging case; a mutant that unlinks at apply fails it |
| B3 | yes | gap | The bundle carries no application state. D16 sets the volume counter "from the checkpoint, not from the directory", and replay also needs the current volume and the open chunked commands with their terms (D12's abort rule), but `strata:checkpoint-bundle` lists only the manifest's position, term and CRC32C and each file's name, length and CRC32C. A counter derived from the names diverges after an aborted upload | Job 261117-022356-9bafce: `counter-from-dir` fails 26 crash points (counter 1 against 2) | An opaque, length-prefixed application-state section in the bundle's bytes, encoded by the application, stored and returned by the group. A1's application keeps its counter, current volume and open uploads there, and runs the aborted-upload case |
| B4 | yes | gap | `ack=quorum` completes at commit, before apply, but apply can refuse a committed command: D3's command carries an expected version, and strata's apply answers ten verdicts (`index.hh:88`). Nothing says what the group does with them. An environment verdict (`no_space`, `flush_owed`, `drain_owed`, `map_owed`) is per replica, so skipping it diverges A2's digests; a semantic one (`version_conflict`, `schema_conflict`, `fields_conflict`, `invalid`) makes a commit-time success false. Pipelined prepares produce it, and so does a leader change | Job 261117-022605-b29cff: a leader preparing against its applied state acknowledges a refused write in 20 schedules; with a map of its own pending versions, in 5 (the old leader's entry is still unapplied at the new leader); with that map and no prepare before its term's NoOp (`core.h:1146`) has applied, in 0 | State in D3, D4 and group-core's API: (1) apply is total: owed verdicts run their chore and retry (document-gate D6), `no_space` stalls that replica's apply and reports it, never skips, and any other refusal of a committed command fences the replica (`application_failed`); (2) the group exposes a leader's ready point (applied through its term's first entry), so a leader that prepares from its pending versions after it never has an acknowledged command refused. Alternatively, quorum acks complete from the group's own Committed stream by placement (index, term) and carry apply's verdict, which also avoids `committed()`'s `compacted` after a fast checkpoint. A3 adds two concurrent writes to one id with an election between, every schedule, and a mutant that prepares before the ready point fails it |
| B5 | yes | gap | A6's "an idle open group costs nothing beyond the driver's row" cannot hold: an idle open strata index alone holds 2 descriptors (its directory and owner lock) and about 5.4 KB of heap. The design does not say whether an idle open group keeps its index open, nor who owns that lifetime (Xapiand's pool or strata's catalog) | strata `docs/perf/small-indexes.md:276-284` at 491a1bdc | Restate A6: an idle open group holds no thread, timer, frame or descriptor of the group's own beyond the driver's row and its open index; memory, descriptors and idle CPU per group are stated at 10^4 for quiescent and closed groups, with a close's cost (its checkpoint's syncs); a closed group holds zero descriptors. Name the index's owner, and route every strata commit of a group's index (the catalog's commit hook included) through the group's checkpoint |
| B6 | yes | gap | A1's crash points reach only the journal. HeldCompletion holds the journal's completions, but strata's files go through the synchronous `kronuz::journal::IO` and the file set through the application's own IO, both outside it, and a process crash keeps their unsynced bytes, so the order in B1 (4) goes untested. A1's oracle, "equals the uncrashed run", is undefined for proposals whose outcome the crash left unknown | Job 261117-022356-9bafce: `publish-first` fails 21 crash points, all under power loss, none under process crash | A1 runs over strata's faultfs (`include/strata/core/faultfs.hh`: unsynced sectors and directory entries lost) for strata, the file set and the journal host, with HeldCompletion choosing the cut, and crashes again inside recovery. The oracle replays the recovered log's committed commands from empty, and every acknowledged command is in it. The publish-before-sync mutant fails it |
| N1 | no | gap | A5's observable: the turn log copies live records forward past `retain_segments` (`turn_log.h:303-307`), so "pins no segment" holds even for a group never checkpointed. `Host::close_group` forgets every record after the sequence it is given without checking it is the group's last, and turn-seal A7's guard covers `TurnLog::append` only, so a close with a stale sequence loses entries and still pins nothing | journal `host.h:321-328` on `main` and on turn-seal b08212f | A5 asserts that `TurnLog::read` finds no record of the closed group and that a reopen recovers every acknowledged entry. The lead routes the same guard for `close_group` to turn-seal or `consensus-quiescence` |
| N2 | no | gap | A7 cannot pass at consensus:57745e8: Core becomes leader only once its vote's persistence completes, then appends its NoOp, so a reopened lone voter's first write waits two barriers | `core.h:510`, `:1136`, `:1146` | A7 is a consumer check of `consensus-quiescence` A5; the group links its first `propose` before it awaits `recover`, so the records can share a turn |
| N3 | no | nit | At `rf = 1`, `none` and `quorum` wait for the same barrier (`ProposalPlaced` and commit coincide), unlike document-gate's `ack=none`, acknowledged at apply's return before the barrier (`standalone-turn-log.md` D4). Quest 10 A4 (Q14, against the WAL's async default) likely loses on one client's latency | DRIVER.md, "What completes when" | The lead weighs Q14 before quest 10. A3 runs three voters with the followers' appends held: `none` completes, `quorum` does not until one follower's append completes |
| N4 | no | nit | `read_applied` is sound on Core: each read gets its own probe after its admission (`core.h:782-797`), pending reads get `NotLeader` on step-down (`:449-454`) and `NotReady` before the term's first commit (`:785`) | `core.h` at consensus:57745e8 | The group maps both rejections to a redirect, calls `applied` only once the strata write and the file bytes are visible, and A4 adds a mutant that calls `applied` early |
| N5 | no | nit | `Restore` runs inline on the loop inside `Worker::recover` | `worker.h:147-166`, `driver.h:274-279` | It only decodes the bundle; truncation, deletion and the strata open run on the group's chore before the first Committed range |
| N6 | no | gap | Volume rotation and CRC32C finalization must happen only at points the log decides, never at a checkpoint or on a timer, or replicas allocate different volumes; recovery's deletion must stay inside the file set's directory | D16 | State both in the file-set contract; A2's digest includes the file set |
| N7 | no | gap | The outcome's X10 claim (apply leaves the loop) has no criterion, and the proof standards' named interleavings are missing | quest file, Outcome | A test where a held apply delays no other group's step; capture against apply, close against a Committed range, and `read_applied` against apply, under TSan |
| N8 | no | nit | consensus:57745e8 pins journal 7c08540; document-gate moves strata's journal pin to turn-seal's landing, and strata's declaration wins | consensus `CMakeLists.txt:17`; strata `CMakeLists.txt:29` | Run group-core's suite on that journal before landing; A2 also runs five voters, with its seeds and crash points stated |
| N9 | no | nit | Under B1's rule the bundle's manifest CRC32C cannot be compared with the head, which later commits and merges change | REPLICATION.md "Checkpoints and repair" | State its one use (install's pin) or drop it |

## Not run

- No code exists, so nothing was built, and no criterion's test was written or run.
- The models are Python, not strata: they check the recovery rule and the ack rule, not an
  implementation, and the bundle model's crash semantics are coarser than faultfs's (whole unsynced
  writes lost, no torn sectors).
- `close_group`'s missing guard is read from code, not executed.
- No idle-cost measurement; B5 cites strata's measured numbers.
- `detent:consensus-quiescence` is unbuilt; only its quest file was read.
