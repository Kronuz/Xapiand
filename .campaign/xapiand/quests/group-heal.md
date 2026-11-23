# group-heal: Replication groups grow, heal and run in the simulator

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status group-heal`.

Whole feature (replan-2026-11-20, section 1 F1). Folds `group-install`, `group-sim` and the release
scope of `phase3r` (its A1 to A4 and A7).

## Outcome

A `strata::group` grows and heals. A learner installs a checkpoint, the segment pack and the
application file set in one transfer, catches up from the log and is promoted. Voters are replaced
and removed one step at a time. A damaged segment, manifest or file fences its replica, which
reinstalls at or below commit. A witness stores the log, never applies, and leads only to hand off
(D113 Q3 (a)). strata's deterministic simulator, ported from `design/simulator` onto `main`, drives
the group through crashes, power loss with torn and reordered writes, partitions, clock skew and
membership changes, and its checker proves linearizability over write and `linearizable` get-by-id
histories. It is the Phase 3R item on the release's scope and the group Xapiand's replicas need.

## Acceptance criteria

Gate: the suite (`build-test`) and CI (`asan`, `tsan`). Every criterion below is a test the suite runs.

- A1 A learner installs a 64 KiB-chunked segment pack and file set from a pinned manifest, catches up
  and is promoted; replace and remove pass across leader changes; digests, file set included, equal
  the leader's; a mutant install that skips the file set fails, and a retried install sends only the
  files the learner lacks or holds short, counted in bytes: tests with values, the mutant a
  `--fails-without` job
- A2 Catch-up after a checkpoint: `group-core`'s schedules at seeds 8, 12, 16, 18, 23, 24 and 27
  (3 voters), plus 40 new seeds of the same shape, settle with checkpoints taken while a replica is
  down (D109); a mutant that drops `SnapshotNeeded` fails them: tests
- A3 A damaged segment, manifest or application file fences its replica, which reinstalls at or below
  commit and serves no corrupt byte, one test per file class; a simulated device that persisted a
  non-prefix subset of its writes since the last flush is answered by the journal's checksums and
  this repair: a test with values
- A4 A two-data-voter-plus-witness group survives the loss of any one member; the witness stores the
  log, never applies, and leads only to hand off: simulation-style tests
- A5 The simulator runs in the suite at a short budget per voter count (3 and 5), covering every
  fault class of the old `group-sim` A1 (crashes, power loss at every barrier with torn and
  reordered writes, partitions, clock skew, add, remove, replace, D7's 0-to-1 growth window, chunked
  commands cut by a leader change, corruption of every file class); the checker passes on write and
  `linearizable` get-by-id histories and checks `prefix` reads for a consistent prefix only; each
  mutant fails it (`--fails-without`): an apply that skips one committed entry; a recovery that
  ignores the checkpoint's file-set lengths; a `read_applied` without the quorum probe; a leader
  that applies an abandoned chunk of a lower term
- A6 [artifact] `docs/mechanisms/group-install.md` and `docs/mechanisms/simulator.md`

## Tasks

One agent each, merged into the quest branch on its own tests (`campaign start group-heal --task T
--files GLOBS`). The lead sets exact globs from `group-core`'s layout at each `campaign start`.
Tier and effort follow the skill's implementation row (strong, high: durability and concurrency).

| Order | Task | Files | Assignment, tier, effort | Does |
|---|---|---|---|---|
| 1 | `sim-port` | `sim/**`, `docs/mechanisms/simulator.md`, `CMakeLists.txt` only for `sim/`'s `add_subdirectory` line | implementation, strong, high | Ports engine, network, history and checker from `design/simulator` onto main against `group-core`'s quest-file contract; its own tests; builds with `sim/CMakeLists.txt`. Starts first |
| 1 | `install` | `include/strata/group/install*`, `src/group/install*`, `test/group/install*`, `docs/mechanisms/{replication-group,group-install}.md` | implementation, strong, high | Learner, install transfer, catch-up, promote, replace and remove (A1, A2) |
| 2 | `repair` | `src/group/repair*`, `test/group/repair*` | implementation, strong, high | Fencing and reinstall of a damaged replica (A3), on `install`'s contract |
| 2 | `witness` | `src/group/witness*`, `test/group/witness*` | implementation, strong, high | Witness voter (A4), on `install`'s membership steps |
| 3 | `sim-rows` | `sim/**` (after `sim-port` merged), `test/group/sim*`, `docs/gates/phase-3r.md` stub | implementation, strong, high | Drives the group through every fault class at short budgets, the checker rows, the four mutants and the non-prefix device (A3's simulator half, A5) |

Order 1 tasks run together; order 2 starts once `install`'s contract is merged; order 3 once the rest
are merged. The quest ends with the suite and CI green at the merged head.

## Contracts

Provides `strata:group-install`: the install transfer (segment pack plus file set, bytes on the wire)
and the membership steps (add learner, promote, remove, add witness). Consumes `strata:group` and
consensus's `SnapshotChannel` and fetch lane. `format-freeze` reviews the transfer. The simulator
consumes `strata:group` through its public C++ API only and provides nothing on disk or wire.

The Driver's install surface comes from Detent's `consensus-driver-install` (msg-187; contract in
consensus `docs/DRIVER.md`, section Install, at consensus:2c598fb): with `GroupOptions::snapshots` set the
Driver owns the group's SnapshotChannel; the Sink gets `SendTransfer` (host sends it with
`transport::send_transfer`), `ValidateImage` (stage here, answer `validated`/`validation_failed`) and
`ActivateImage` (replaces `ActivateInstall`; the image is borrowed for the call); received transfer
payloads go to `transfer()`, the rest to `receive()`; the whole transfer finishes within
`Timing::snapshot_timeout`. The install task binds to it once that code lands.

## Decisions

- Lead, 2026-11-23: a damaged replica reinstalls through consensus's install request
  (consensus-request-install, `consensus:install-request`): the fenced follower asks its leader
  for an install and takes install's path end to end, with the image as its new base. One
  mechanism for every replica that needs state, and no new strata wire. The repair task builds on
  it once that quest lands; options (a) and (b) are not built.
- Design D7 (short groups are one voter plus learners, never two voters), D16, F1, F9, F24 (no
  scrubber for the release: reads and installs fail closed). Pre-release bytes (D30, D87).
- Q3 (a) is the lead's default (`rf = 2 + witness`); the owner may still change it.
- Needs `group-core`, which shares its files; starts when it lands. `placement` lands after this.
- `track-d` gives up `sim/**` and `docs/design/simulator.md` (it is parked).
- Parked, no hardware: old `phase3r` A5 (quorum ingest against Elasticsearch on NVMe), A6 (fsyncs at
  1,000 groups, after the release by D125) and A8 (FreeBSD and arm64 rows; D35, D36).
- Lead, 2026-11-21 (task `repair`, A3): a damaged replica reinstalls through install's own path. A
  follower asks its leader for an install (a consensus Driver call that makes the leader treat it as
  below its base), so there is one mechanism for a replica that needs state and no new strata wire.
  Rejected: a `repair/pin` fetch name (new bytes, races sustained commits) and a membership
  replacement per repair (a membership change and a whole-replica transfer each time). The Driver
  call lands in consensus first (asked of Detent's lead, msg-229; default quest
  consensus-request-install); `repair` builds on it, and sim-rows goes ahead meanwhile.

## When done

Runs once, when the feature is complete and before it lands; findings are fixed on the quest branch,
each with a test that fails without it.

- Review: one reviewer, lens `durability`, carrying correctness (install bytes on the wire, repair).
- Gate rows (Phase 3R, old `group-sim` A1): at least 10^7 steps over at least 1,000 seeds for 3 and 5
  voters, as `--later` light jobs at priority -1 on the macOS host (deterministic, no timing), in
  chunks of at most 30 minutes; old `group-install` A4, 100 installs of a 1 GiB file set on macOS,
  a light job. The lead writes `docs/gates/phase-3r.md` from them.
- Old `group-install` A7's quiescence consumer test (`detent:consensus-quiescence`) is a `--later`
  check here, per the re-plan's Detent recommendation.

## May land early

The simulator port (`sim/` with its own tests and no gate rows). Nothing else: the install transfer
is a wire format.
