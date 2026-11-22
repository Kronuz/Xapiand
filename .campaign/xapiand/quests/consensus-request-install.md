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
- A2 A request from a voter that is current (its log matches the leader's at its commit) is
  refused, or answered as a no-op, and never makes the leader ship an image: a test that sends the
  request repeatedly from a current voter and counts zero transfers, plus a mutant without the check
  that a named test kills (Detent's lead, msg-235: no full-image loop from a confused follower)
- A3 The request survives leader change and races: a leader that loses leadership mid-install, a
  second request while one install is in flight (one install), and a request to a non-leader
  (refused with a redirect or a named error): tests in the `suite` check
- A4 `Driver::witness_bound()` returns Core's bound, and a witness that truncates through it keeps
  every record it must hold: a test, plus a mutant truncating one past the bound that a named test
  kills
- A5 [artifact] `docs/mechanisms/install-request.md` describes the request, the current-voter
  refusal, the races and the cost (one image per real need)
- A6 The `asan` and `tsan` checks pass, and CI is green on every platform: [ci]

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
- Lens: correctness, with the protocol's races and the image loop as its questions.

## May land early

None: a protocol change in a shared library.
