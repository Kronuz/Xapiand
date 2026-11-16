# group-core: strata::group: propose, apply, checkpoint with a file set, read, close, recover

Risk: critical. The lead writes this quest; contributors read it and never edit it. Its machine
state (status, branch, checks, reviews) is in `state.json`: `campaign status group-core`.

## Outcome

`strata::group` exists: one replication group per shard on Detent's coroutine drivers
(`Driver<Sink>`, D97 Q2), at every replica count, a lone voter included. It proposes commands,
applies each committed entry in log order from the Sink in a chore that leaves the group's loop
(Xapiand X10), checkpoints with a bundle that names the strata manifest and an application file set
(design D16), recovers from the bundle's applied position, reads through `read_applied` after the
driver's quorum-probe read (D4), and closes only after a checkpoint covers its applied position
(D11). Document mode only (D3); segment mode follows the release. strata pins consensus here.

## Acceptance criteria

- A1 A lone voter's create, propose, apply, checkpoint and recover through `HeldCompletion` at
  every crash point, with an application file set in the bundle; recovery truncates each file to
  its checkpointed length, deletes files the bundle does not name, replays from the manifest's
  applied position, and the logical-content digest, file set included, equals the uncrashed run
  (the `crash` gate check, and a job)
- A2 Three voters in process: every `quorum`-acknowledged entry survives any one crash, and digests
  are equal after every schedule (a job)
- A3 `ack=none` completes at `ProposalPlaced` and `ack=quorum` at commit (a test with the event
  order asserted)
- A4 On a leader partitioned away and deposed, `read_applied` after `Driver::read` never returns a
  state older than a committed write; a mutant that skips the probe fails (`--fails-without`, a
  simulation-style test)
- A5 A closed group pins no turn-log segment; a mutant that closes without a checkpoint fails
  (`--fails-without`)
- A6 An idle open group costs nothing beyond the driver's row, and a closed one zero descriptors;
  memory, descriptors and idle CPU per group stated at 10^4 groups (an `exclusive` job)
- A7 A reopened lone voter's term record and its first write share one turn: the barrier count of
  that write equals a write to an open group (a test counting barriers)
- A8 [decision] The release's level set: cluster-release Q2, pending owner; lead default (a),
  document mode with `quorum` and `none`
- A9 [artifact] `docs/mechanisms/replication-group.md`

## Contracts

Provides `strata:group` (C++ API: create, open, propose, `read_applied`, checkpoint, close) and
`strata:checkpoint-bundle` (bytes): the manifest's applied position, term, CRC32C (REPLICATION.md
G6), and the application file set: name, length at the applied position, CRC32C once immutable.
Consumes `consensus:driver` and `journal:driver` from Detent's quests, and the closed state's
hard-state bytes from `detent:consensus-quiescence`. `format-freeze` reviews the bundle.

## Decisions

- Design D2, D3, D11, D16; F14 (close after a checkpoint), F15 (reopen shares the first write's
  turn).
- Design section 5: phase3r A1 and A7 are this quest's A1, A2 and its sanitizer checks; the lead
  narrows `phase3r`, which holds `include/strata/group/**` and `test/group/**`, before this
  starts.
- Lead: strata is header-only, so the plan's `src/group/**` is dropped; tests under `test/group/`
  are globbed by the prerequisite quick fix, and `CMakeLists.txt` stays here for the consensus pin.

## May land early

None: it defines the checkpoint bundle's bytes and a durability mechanism.
