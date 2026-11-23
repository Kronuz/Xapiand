# consensus-request-install: consensus: a follower asks its leader for an install; the Driver exposes witness_bound

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-request-install`.

## Outcome

A replica whose state is damaged or lost gets it back through install's existing path. A follower
asks its leader for an install with a Driver call, the leader treats it as below its base, and the
snapshot channel ships an image that becomes the follower's new base. `Driver::repair(b)` rewinds
only within [own base, own commit], so it can't recover lost files; this closes that gap with no new
wire beside the request itself. The Driver also exposes `Core::witness_bound()` (core.h:265), so a
witness can compact its log (D9). group-heal's `repair` task (strata) builds on both (ruling in
.campaign/xapiand/quests/group-heal.md, Decisions).

## Acceptance criteria

- A1 A follower's install request, from a follower whose log or state is behind or damaged, makes
  its leader ship one install through the snapshot channel, and the follower ends at the image's
  base with its delivered ranges intact: tests in test/driver_install_request_test.cc (the `suite`
  check)
- A2 No image loop: a leader ships at most one image per requester per leader base. A request
  whose base is not below the leader's, from a requester that already received an image at the
  leader's current base, is refused as `Current` and ships nothing. A damaged replica whose base
  equals the leader's gets its one image. Tests: a current voter sends the request repeatedly and
  the leader ships zero images, and a damaged replica at the leader's base sends it repeatedly and
  gets exactly one; plus a mutant without the check that a named test kills (Detent's lead,
  msg-235: no full-image loop from a confused follower)
- A3 The request survives leader change and races: a leader that loses leadership mid-install, a
  second request while one install is in flight (one install), and a request to a non-leader
  (refused with a redirect or a named error): tests in the `suite` check
- A4 `Driver::witness_bound()` returns Core's bound, and a witness that truncates through it keeps
  every record it must hold: a test, plus a mutant truncating one past the bound that a named test
  kills
- A5 [artifact] `docs/mechanisms/install-request.md` describes the request, the current-voter
  refusal, the races and the cost (one image per real need)
- A6 The two new frames are wire changes: `InstallRequest` and `InstallRequestResponse` join
  `Message` with their codec frame kinds, and the change is named in the mechanism note with its
  version handling (`wire_version` in node_hello.h bumps, or the note says why it needn't). The
  codec round-trips both (test/transport/codec_test.cc), `fuzz/seed.cc` seeds both kinds into
  `fuzz/corpus/decoders`, and `fuzz/fuzz_decoders.cc` runs at least 30 CPU-minutes under ASan and
  UBSan with no finding (Detent's lead: decoder fuzz is a high-risk gate)
- A7 The `asan` and `tsan` checks pass, and CI is green on every platform: [ci]

## Contracts

- Provides `consensus:install-request`: the Driver call, its refusal reasons and the request's
  frame, plus `witness_bound()`. Bytes may change until the first release (formats open until
  release).
- Detent consumes the Driver. Its lead reviews the API (the header diff) before the body is written
  (msg-235), and consensus-driver-flow and driver-pipeline order after this quest.

## Decisions

- Lead, 2026-11-21: Detent's lead took the default (msg-235): a quest of ours in consensus, ordered
  after detent:consensus-quiesce-election-stall (fix round 1 of 2 in core.h's wake rule), not
  folded into driver-pipeline.
- Lead, 2026-11-21 (API at consensus:c7a1ad7, its Q1 and Q2): "current" can't mean "log matches at
  its commit", since every honest follower matches there, the damaged one included. A pure base
  comparison refuses a damaged replica at the leader's base until the leader checkpoints. So the
  rule is one image per requester per leader base (A2): loop-free by construction, bounded by base
  moves and leader changes, and it serves the repair case. Sent to Detent's lead with the header diff.
- Detent's lead, 2026-11-21: APPROVE the API shape at consensus:c7a1ad7, with two header fixes before
  the bodies. (1) `Current`'s docs in types.h (InstallRequestReply) and driver.h (reinstall) state
  the per-requester-per-base rule, and that the leader keeps one entry per member, dropped at a
  leader change. (2) reinstall's Fails-with list maps `NotMember` (a removed incarnation) to a
  `RejectReason` (`InvalidRepair`). The new frames' codec change and decoder fuzz are A6.
- Lens: correctness, with the protocol's races and the image loop as its questions.

## May land early

None: a protocol change in a shared library.
