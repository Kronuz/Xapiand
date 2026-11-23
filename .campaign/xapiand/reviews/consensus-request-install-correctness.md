# consensus-request-install review: correctness

Commit: consensus:1443159aae93fc74d201bd61cf49569098ec9cd2
Verdict: CHANGES
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261122-211822-2168f0: cases, codec round trip and corpus replay at consensus:1f21967. The merge
  to 1443159 touches no file of this quest's protocol code beyond main's own changes.
- 261122-211813-967b3b: decoder fuzz, 30 CPU-minutes under ASan and UBSan, no finding (A6).
- 261122-211833-8e72b9 and 261122-211833-e92f60: the A2 loop-guard mutant and the A4 bound+1
  mutant, both killed by named cases.

## What I checked myself

Read the diff in core.h, driver.h, worker.h, codec.h, types.h and admission.h against the five
questions. I ran one probe, `.scratch/xapiand/review-consensus-request-install-correctness/probe.sh`
(it adds `lost_record.cc` as a case of test/driver_install_request_test.cc, Debug build): job
261122-235756-e57e70.

1. Image loops: none. `imaged` lives in the leader's `Peer`, so it survives a follower restart.
   It is dropped with `peers_` at a leader change (`follower()`) and at a member's removal
   (core.h:1395-1400). It is bounded at one bool plus one Index per member. When the base moves
   mid-flight, replicate restamps it (core.h:1481), so the result is one image per base. F1 is
   the other side of that state.
2. Races (A3):
   - A change of leader or term before admission drops the request as `NotLeader` (`follower()`,
     `campaign`).
   - After admission, the install owns the request.
   - A non-leader answers `NotLeader`.
   - These paths are tested, and delivery resumes after each drop (handle(Receive), Persisted).
3. Follower state:
   - An image at or below commit is taken only when `asked_by` and `retains_suffix` both hold.
   - The commit index is kept.
   - Activation fences if a delivered range is outstanding, and delivery is blocked while a
     request is pending.
   - A crash after PersistInstall recovers at base = applied = boundary, with the log kept, and
     re-delivers. A crash before it leaves the old state, with no request record (F1).
4. `witness_bound`:
   - It is read on the loop like `state()` (DRIVER.md: plain calls, loop thread).
   - The only truncation path is LocalCheckpoint's witness clause.
   - A witness's RequestInstall is `InvalidRepair`, and the leader answers a witness `NotMember`.
5. Wire:
   - `wire_version` stays 1, and the note gives the reason.
   - host_transport.h:1625 drops a frame it cannot decode.
   - The decoder bounds every field through get64 and checks the result range and trailing bytes.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A damaged replica at the leader's base can lose its one image and never get it. The leader sets `imaged = base` when it grants (core.h:1165, 1481), and it clears `requested` on a CaughtUp release (core.h:800). The follower's request record (`reinstall_`, core.h:1605) lives only in memory, so a restart drops it, and so does a call failing `NotLeader` with its answers lost. The leader's retry image then lands with no request pending, is refused `CaughtUp`, and counts as delivered. Every later call is `Current` until the leader checkpoints or leadership moves. This breaks A2's "a damaged replica whose base equals the leader's gets its one image". | Job 261122-235756-e57e70: the first call fails `NotLeader`, the retry raises a second SnapshotNeeded, no activation, and the second call fails `Current` (reason 11) under the same leader. | Count only a whole answer to the image: set `imaged` on `Installed` and `Rejected`, and leave it unchanged on `CaughtUp`, which means the requester no longer asked. A confused follower still cannot loop, since a pending request accepts its image. Each extra image needs a fresh host call after a lost record. Add `lost_record` to the suite. |
| N1 | no | nit | The note says an older leader "drops or refuses" kinds 10 and 11. peers.h's reader (peers.h:726) throws on an unknown kind and closes the control connection, so an older peer resets the link on every resend until `election_max`. This is harmless while one build runs per cluster. | peers.h:716-727 | Say so in the Wire section, or bump `wire_version` after the first release, as the note already plans. |
| N2 | no | gap | Under a transfer that fails every time (`Unavailable` or `TimedOut`), `requested` stays set and each resend gets `InFlight`, which restarts the follower's wait (core.h:1172). The call never completes, and the leader keeps the peer on the snapshot path. A lagging peer already behaves this way, but here it also holds a Driver call open. | core.h:800, 1172 | Optionally bound the call, for example fail `NotLeader` after N snapshot timeouts, or document it under Races. |

## Not run

- No TSan, ASan, or Linux or FreeBSD run of my own; those belong to CI (A7).
- No real restart through the Driver harness. F1's probe drops the record by losing every answer.
  A restart drops the same in-memory record, so I traced that path by reading the code.
- No rerun of the jobs queued at the merged head.
