# consensus-request-install: journal

State: waiting on jobs 261121-223440-971166 (cases+codec+replay), 261121-223618-6521b4 (A2 mutant), 261121-223622-85de99 (A4 mutant), 261121-223626-0f7cf8 (A6 fuzz); CI suite/asan/tsan at consensus:fc4af4f (pushed).
Quest: .campaign/xapiand/quests/consensus-request-install.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Bodies, tests and docs at consensus:fc4af4f, pushed; nothing compiled yet except by the jobs
in State (machine work only through jobs).

## Next

1. Fix what job 261121-223440-971166 shows; then push and run `campaign verify` (suite, asan,
   tsan on CI), the two mutant jobs and the 30-CPU-minute decoder fuzz.
2. Merge consensus main once detent:consensus-quiesce-election-stall and
   detent:consensus-peers-wake land (merge, never rebase). Expect one conflict: Core's
   next_deadline follower line (theirs adds acknowledge_at_, mine install_deadline()); keep both
   in one std::min.

## API (consensus:c7a1ad7)

- `Driver::reinstall(GroupId, cancel_token) -> task<RepairCompleted>`: a follower asks its leader
  for an install. It completes as `repair` does: at the boundary, with the epoch advanced and
  re-delivery from its own log.
- Refusals, as `rejected{reason}`:
  - `Current` (new): the leader's base is not above this replica's.
  - `NotLeader`: no leader is known, the one asked does not lead, or leadership changed first.
  - `Busy`: an install, a persistence, a delivered range or another request is in progress.
  - `LeaderMustTransfer`: this replica leads.
  - `InvalidRepair`: this replica is a witness.
- Frames: `InstallRequest{term, request, base}` and
  `InstallRequestResponse{term, request, result, leader}`. The result is one of `Started`,
  `InFlight`, `Current`, `NotLeader` (with `leader` as the redirect) or `NotMember`.
- Core event: `RequestInstall{request}`.
- `Driver::witness_bound(GroupId) -> optional<Index>`: Core's bound, absent for a group that is
  not open.

## Design

The leader decides by bases. The request carries the requester's own base boundary. When that
base is below the leader's, the leader ships: it sets the peer's `lacks_base` and puts `next` at
its own base, so `replicate` raises `SnapshotNeeded` once nothing is in flight. That is the
existing path. The leader leaves `matched` alone, so verified progress never rewinds. Otherwise
it answers `Current`.

The follower takes the image under a pending request. Today it refuses any image at or below its
commit index as `CaughtUp`. While its own request to that leader in that term is pending, it
accepts such an image if it retains the boundary's entry. It keeps its log suffix and its commit
index, and moves its base and applied cursor to the boundary. It then advances the epoch and
re-delivers (boundary, commit] from its own log. No acknowledged entry is dropped, so quorum
durability holds. An image it did not request stays `CaughtUp`, as today.

While the request is pending, the follower:
- Delivers nothing.
- Refuses `LocalCheckpoint` and `Repair` as `Busy`, so damaged state never becomes a base.
- Resends the frame every heartbeat until the leader answers, and once more after `Started` if no
  image is admitted within `snapshot_timeout`.

There is no image loop. An install raises the requester's base to the leader's, so every later
request gets `Current` until the leader checkpoints past it. A voter whose base is at or above
the leader's never gets an image, however often it asks (A2). The worst case is one image per
leader checkpoint, and only when the host calls `reinstall`. The leader holds one snapshot per
peer, so a request and the lagging path never ship two images at once.

Races (A3):
- The leader loses leadership mid-install:
  - It releases the snapshot (`LeadershipLost`).
  - The follower's pending request is bound to (leader, term). If the leader or term changes
    before the follower admits that leader's image, the call fails `NotLeader` and the host asks
    the new leader.
  - An image already persisting (`install_` set) finishes and completes the call. Its boundary is
    committed, so it is valid under any later leader.
  - A stale image that arrives after the new term is refused `StaleTerm`, as today.
- A second request while one is in flight:
  - A second `reinstall` on the same group is `Busy`.
  - Resent frames get `InFlight` while the peer's snapshot is set. A late frame gets `Current`
    once the install is done.
  - Either way there is one install.
- A request to a non-leader:
  - The responder answers `NotLeader` with the leader it knows, and the call fails
    `rejected{NotLeader}`. The requester's Core learns its leader from appends, as usual.
  - A leader that is a witness answers `NotLeader`: it holds no image and is handing off (D9).
- The leader's base moves while the source is pending: the existing `Stale` release applies.
  `lacks_base` stays set, so the leader retries at the new base.

Wire and cost: two control frames.
- `InstallRequest`, tag 10: term, request, base index and base term.
- `InstallRequestResponse`, tag 11: term, request, result and leader.

That is one image per real need. The request is resent at the heartbeat interval only until it
is answered.

Out of scope: the strata side (group-heal's `repair` task). `docs/DRIVER.md` gains the call.

## Questions for Detent's review

- Q1, what "current" means (A2). The quest says "its log matches the leader's at its commit".
  Every honest follower's log matches there, including group-heal's damaged replica, whose log is
  intact and whose state is lost. So the operative test is the base: a requester is current when
  its base is at or above the leader's. Default: the base test.
- Q2, the equal-base gap. A damaged replica whose base equals the leader's is refused `Current`
  until the leader checkpoints past it. This happens, for example, in a group closed at rest,
  where every base sits at the commit index.
  - Alternative: the leader remembers, per peer, the base it last installed and allows one image
    at an equal base per leader term. That is still bounded, but a current voter at the leader's
    base could then pull one image, against A2's zero.
  - Default: keep the strict rule and name the gap in A5's doc.

## Tests (test/driver_install_request_test.cc, one ctest per case)

- A1 `lost_state`: a follower calls `reinstall` with a current log and a base below the leader's.
  It sees one ValidateImage and one ActivateImage, and ends with base = applied = the image's
  boundary. Its log above the boundary is intact (the leader's `matched` is unchanged), committed
  (boundary, commit] is re-delivered, and the epoch has advanced.
- A1 `behind`: a follower whose log lags below the leader's base calls `reinstall`. It gets one
  install and catches up by appends.
- A2 `current`: a voter whose base is at or above the leader's calls `reinstall` 100 times. Each
  call is `rejected{Current}`, and the leader emits zero SnapshotNeeded and zero SendTransfer.
  Mutant: remove the leader's base comparison; `current` kills it.
- A2 `after_install`: after one install, repeated `reinstall` calls are `Current`, and the
  transfer count stays at 1.
- A2 `unrequested` (Core level): an image at or below the commit index, with no request pending,
  is still `CaughtUp`.
- A3 `leader_lost`: leadership moves after SnapshotNeeded, and again mid-transfer. The call fails
  `NotLeader` or completes, and never with two images. A `reinstall` to the new leader then
  succeeds.
- A3 `second_request`: a second `reinstall` while one is pending is `Busy`. Resent frames get
  `InFlight`, and there is one transfer.
- A3 `non_leader`: a frame to a follower gets `NotLeader` with its leader. The call is
  `NotLeader` when no leader is known, `LeaderMustTransfer` on the leader and `InvalidRepair` on a
  witness.
- A4 `witness_bound`: in a witness group, `Driver::witness_bound` equals Core's. A witness
  checkpoint through the bound is accepted and keeps every record above it. One past the bound is
  `InvalidCheckpoint`. Mutant: let the witness clause in `checkpoint_rejection` allow bound + 1;
  `witness_bound` kills it.

## Blocked on

Lead (file set): the bodies need files outside this quest's set.
- `include/consensus/transport/codec.h`: wire tags 10 and 11. Its `static_assert` refuses a new
  Message alternative without them.
- `include/consensus/admission.h`: admission plans for the two frames and for `RequestInstall`.
  Its `static_assert` refuses either without them.
- `include/consensus/worker.h`: the `RequestInstall` busy gate beside Repair's (worker.h:745), and
  a `witness_bound()` accessor. The Driver reaches Core only through the Worker.
- The codec round-trip test, in whichever test file holds it: cases for the new tags.
- `docs/DRIVER.md`: the call's entry.

Until then, the frames and the event are declared but are not part of `Message` or `Event`, so
the tree compiles. The wider file set is needed before step 3, not for the review.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-223440-971166 (lost_state, behind) and the suite check on CI at consensus:fc4af4f |
| A2 | job 261121-223440-971166 (current, after_install, unrequested); mutant job 261121-223618-6521b4 (`--fails-without`, after_install) |
| A3 | job 261121-223440-971166 (leader_lost, second_request, non_leader) and the suite check on CI |
| A4 | job 261121-223440-971166 (witness_bound); mutant job 261121-223622-85de99 (`--fails-without`, witness_bound) |
| A5 | [artifact] docs/mechanisms/install-request.md at consensus:fc4af4f |
| A6 | codec round-trip and corpus replay in job 261121-223440-971166; fuzz job 261121-223626-0f7cf8 (30 CPU-min, ASan+UBSan); wire version ruling in the note's Wire section |
| A7 | CI at consensus:fc4af4f (asan, tsan, every platform) |

Messages: msg-235 (Detent's ask) is taken as A2's refusal by base. The API goes to Detent for
review through the lead.

## Result

<pending>

## Rulings I made

- Detent's fixes (msg-235 verdict) are consensus:9ea7c55, header only: Current states the
  per-requester-per-base rule in types.h and driver.h; NotMember maps to InvalidRepair.
- `imaged` records any image the peer got at a base (requested or lagging, at Started and at
  Installed), so "one image per requester per leader base" holds across both paths.
- `requested` survives Unavailable, TimedOut and Stale releases (nothing whole reached the
  peer: the leader retries the same base's image) and clears on Installed, CaughtUp, Rejected.
  A follower that rejects its image gets Current next, never a second image at that base.
- Unanswered for an election timeout, the follower's request fails NotLeader (bounds a lost or
  undecodable frame); answered, it asks again every snapshot_timeout until an image is admitted.
- `wire_version` stays 1 (mechanism note, Wire): formats open until release, Gone (kind 9)
  precedent, and the frames never reach an older node unprompted. Bumping would also break
  test/transport/hello_test.cc's version() case, which uses 2 as "newer", outside this file set.
- reinstall is its own waiter Kind (`reinstall`), so the blocking entry refuses it needs_peer
  like propose; it still completes through RepairCompleted's waiter slot.
- A resting follower's reinstall wakes it through wake() (called, not edited) and suppresses
  rest while pending, so core.h stays off the wake rule's lines.
- Mutants run as `--fails-without` jobs that perl-patch core.h in the job's checkout in the
  build step (driver_mutants.py is outside this file set); a patch that does not apply fails the
  build, so only a complete build then the named test failing passes.

- The call is named `reinstall`, not `request_install`, because `Worker::request_install` already
  means admitting an incoming image.
- The call returns `RepairCompleted`, reusing `Kind::repair`'s waiter and the Sink's existing
  overload. Both restore application state at a boundary and advance the epoch.
- The leader's refusal is one new `RejectReason` value, `Current`, rather than a second error
  type.
- `Driver::witness_bound` returns `optional<Index>`, absent for a group that is not open, as
  `state` does.
