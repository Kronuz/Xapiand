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

- A1 A lone voter over strata's faultfs (unsynced sectors and directory entries
  lost) for strata, the file set and the journal host, with `HeldCompletion` choosing the cut:
  create, propose, apply, checkpoint and recover at every crash point, the windows between
  strata's commit and publication and between publication and the retired files' unlink included,
  and a second crash at every point of recovery. The run covers a staging file consumed above the
  bundle, an aborted upload, an upload open across a checkpoint, a cancelled checkpoint, a strata
  commit outside the group and a refused write, with the application's counter, current volume
  and open uploads in the bundle's application state. Oracle: every acknowledged command is in the
  recovered log, and the digest (documents with versions resolved through their locators, the file
  set, the application state) equals a replay from empty of the recovered log's committed
  commands; reads open only at P_H and match that replay through it. Mutants, each
  `--fails-without`: replay the file set from P_H; unlink at apply; publish before the files sync;
  capture inside an entry's apply; file effects gated on strata's verdict (the `crash` gate check,
  jobs)
- A2 Three voters in process: every `quorum`-acknowledged entry survives any one crash, and digests
  are equal after every schedule (a job)
- A3 `ack=none` completes at `ProposalPlaced` and `quorum` when the proposer applies its
  placement, with apply's verdict; with the followers' appends held, `none` completes and `quorum`
  does not; two merges to one id and a conditional write with an election between, every
  schedule: no acknowledged success refused, no merge answered 409, the conditional write's answer
  matches its effect, and no re-proposal; mutants that acknowledge at commit and that prepare
  before the ready point fail (tests)
- A4 On a leader partitioned away and deposed, `read_applied` after `Driver::read` never returns a
  state older than a committed write; a mutant that skips the probe fails (`--fails-without`, a
  simulation-style test; a mutant that publishes the applied position before strata's write is visible fails it
- A5 Once a group closes, `TurnLog::read` finds none of its records, and a reopen recovers
  every acknowledged entry from the bundle; a close racing a `Committed` range ends with a bundle
  covering applied; a mutant that closes without a covering checkpoint fails (`--fails-without`, a
  test)
- A6 An idle open group holds no thread, timer, frame or descriptor of its own beyond
  consensus's row and its open strata index (2 descriptors, about 5.4 KB of heap); memory,
  descriptors and idle CPU per group stated at 10^4 groups for open idle, quiescent and closed
  groups, with a close's cost (its checkpoint's syncs); a closed group holds zero descriptors.
  strata's catalog owns the group's and index's lifetime: a catalog close of an idle group
  publishes a bundle covering applied (an `exclusive` job and a test)
- A7 `detent:consensus-quiescence` A5's consumer check: a reopened lone voter's term record
  and its first write share one turn, the barrier count of that write equal to a write to an open
  group (a test counting barriers, run after that quest lands; its A5 job is the evidence beside it)
- A8 [decision] The release's level set: cluster-release Q2, pending owner; lead default (a),
  document mode with `quorum` and `none`
- A9 [artifact] `docs/mechanisms/replication-group.md`
- A10 Apply leaves the loop: with one group's apply chore held, another group on the
  same loop proposes, commits and applies; capture against apply, close against a `Committed`
  range and `read_applied` against apply, each with its scheduling points enumerated, under TSan
  (tests)

## Contracts

- `strata:checkpoint-bundle` changes (design revision 2, D16): P_b and term, the file set, an opaque
  application-state section, no manifest CRC32C (install's pin carries it). Consumers build against
  this quest's brief.
- strata `docs/REPLICATION.md` "Checkpoints and repair" takes the journal's proposed text.


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
