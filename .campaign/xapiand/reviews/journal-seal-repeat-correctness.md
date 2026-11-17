# journal-seal-repeat review: correctness

Commit: journal:2c0b92048dd71b8024975a68cc73d1b01986cc0c
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261117-103233-d90dfc (`repeat`: three records at staged+1..+3, each call's room by value, room
  met and one byte over) with mutants 261117-094035-f25876, 261117-094044-57a827,
  261117-094052-389c68 failing without the repeat call, the framing charge and the staged advance.
- 261117-103233-7f09bc (`repeat_turns`) and mutant 261117-094103-c463ab (rotation).
- 261117-103201-cfa134 (`repeat_throw`, `repeat_unseal`, `repeat_threads`), mutants
  261117-094114-d5bc25 and 261117-094124-60920c.
- 261117-103206-61c5cc: every `turn` ctest under TSan, `repeat_threads` included, exit 0.
- 261117-103158-aa11a7 (whole suite, crash fixtures), 261117-103212-a8b3aa (ASan).

These cover the named cases; I did not rerun them. What they leave open: power loss with several
sealed records per turn and a repeat throw, a model check of room and `first` over random
workloads, and what happens to a repeat call's error when no next turn is asked for.

## What I checked myself

Probes in `.scratch/review-journal-seal-repeat-correctness/` of the home checkout (`probe.cc`
includes `test/turn_seal_test.cc` for its helpers; `build.sh` adds the target, `run.sh CASE` runs
it), all at journal:2c0b920:

- 261117-104440-a13c5c `crash`, exit 0: 12 seeds, power cut at every I/O operation (645 cuts) of
  16-turn runs where three sealers each hand over a backlog of 1 to 1500-byte records, several per
  turn, mixed with staged records; seeds 3, 6, 9, 12 throw on a repeat call. Recovery holds every
  acknowledged record and the in-flight turn whole or absent; each turn's and each recovered
  group's sequences are consecutive (no gap, no reuse); the turn after a throw fails with it.
- 261117-104446-44734a `oracle`, exit 0: 20 seeds x 60 turns, 2 to 4 groups with backlogs that
  never empty (small records, records sized to the share, records needing the whole room). Each of
  7657 calls gets exactly `(first ? whole : half) - used`, where `used` charges every earlier
  pending and sealed record's `record_cost`; `first` is true only until the turn's first record
  across groups; no call is offered less than `record_cost(0)`; no turn passes `whole`; 1128
  group-turns sealed several records, 7 met the room exactly. Without staged records (even seeds)
  no group with a backlog waits more turns than there are groups.
- 261117-104449-1701c1 `latent`, exit 1 (the defect, F1): a repeat call throws, nothing waits on
  the group. After the turn: `idle()` true, `failed()` false; `run()` with stop requested returns
  normally; the blocking entry returns normally.

Read: `seal_turn`, `start`, `fail`, `forget`, `unseal`, `idle` (include/journal/turn_driver.h),
`run` (include/journal/blocking.h), `Host::close_group` (include/journal/host.h), and
docs/mechanisms/turn-seal.md.

The quest's five questions:

1. Room: yes. `room_left()` (turn_driver.h:473) is recomputed before every call from `used`,
   which grows by `record_cost` of each accepted record (the framing mutant kills a payload-only
   charge); a repeat call needs `record_cost(0)` (:488) and a record over its room is refused. The
   oracle probe matches every call; the room-met and one-byte-over cases pass in `repeat`.
2. Sequences: yes. `staged` advances per accepted record; an unseal during a call keeps the record
   as the group's via `last_staged`; a throw sets `deferred_` and the driver fails at the next
   `start()` (:437), so no sequence is handed out after it. The crash probe shows replay equals the
   acknowledged records across 645 cuts, throws included. The gap is F1: when that next start never
   comes.
3. `first`: true only until the first sealed record of the turn (oracle). Rotation: a group's
   repeat nothing claims no `resume_at`; the first group not called, or declining its first call,
   gets it (:480, :523). Two groups alternate (`repeat_turns`); with up to four groups none waited
   more than the group count (oracle, even seeds).
4. Lifetime: yes. `Running` (:470, :484) spans all of a group's calls and clears `running_` on
   every exit, after the inner `hold` unlocks (inner scope destroyed first). Each repeat call
   re-reads the table (:491), so no call starts after an unseal or forget returns; an unseal from
   another thread waits for the group's calls. TSan job above is clean.
5. Rulings:
   1. Records before a throw stay: accepted. The failed call's record never enters `pending_`;
      the earlier ones are in the turn that the barrier acknowledges, so acknowledgments equal
      what replay sees (crash probe). Failing the turn unwritten would also be safe; keeping them
      writes what the sealer validly handed over. "The next turn fails" is right only if a next
      turn is guaranteed: F1.
   2. No per-group cap: accepted. The room bounds a turn, and rotation gives the first place to
      the first group not served, so a backlog cannot hold the whole room two turns running while
      another group waits (oracle).
   3. Forget between repeat calls refused: accepted. `forget` is loop-confined, so between repeat
      calls it can only run inside a sealer call, when the group already has a record in
      `pending_`; honouring it would either write a record of a forgotten group or drop one the
      sealer handed over as sealed, unreported. Refusing keeps forget's one rule.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A repeat call's error can pass silently. `deferred_` is invisible to `idle()` (turn_driver.h:237), so after the turn that wrote the earlier records the driver looks idle and healthy: `run()` returns normally on stop (:303), the blocking entry returns (blocking.h:103), `failed()` is false, and `Host::close_group` proceeds. Under ack=none, the quest's own case, nothing waits on the group, so the error surfaces only if something later wakes the driver; a stop or close first loses it, and the failed record stays unwritten with no one told. | 261117-104449-1701c1 (`latent`: idle 1, failed 0, run() outcome '', entry outcome '') | Make the failure due at once rather than at an asked-for turn: either `idle()` is false while `deferred_` is set (run(), the entry and `commit()` then start the turn that fails), or the driver fails right after the earlier records' turn completes and acknowledges them. Test: the probe's `latent` case in `turn_seal_test` (a repeat throw with no waiter: `run()` with stop requested throws the error and `failed()` is true), with a mutant that drops the fix as a `--fails-without` job. |
| F2 | no | nit | The doc's "a record that needs the whole room waits at most one turn behind a group that sealed first" holds for two groups; with more groups each one ahead of it in the rotation can take a turn first (oracle: within the group count), and staged records (not budgeted) can stretch it (oracle odd seeds: worst 6 turns). | 261117-104446-44734a; docs/mechanisms/turn-seal.md Fairness | Say "at most one turn per group ahead of it in the rotation, while staged records leave it the room". |
| F3 | no | nit | An `unseal()` from another thread now waits for all of a group's calls in the turn, up to a full room of records, not one call. Documented; worth a line in the CHANGELOG entry for consumers that unseal under latency limits. | turn_driver.h:470-484 | Optional changelog line. |

## Not run

CI (run 44148860137 was still running); other platforms; the probes under TSan or ASan (the
contributor's sanitizer jobs cover the suite's repeat cases); strata's sealer against this
contract; Detent.

## Re-check of F1 at journal:13592a1

Commit: journal:13592a1 (fix journal:12c5425)
Verdict: APPROVE

The fix: `completed()` acknowledges the turn's records, then fails the driver with the parked error
and rethrows it; `idle()` is false while an error is parked; a waiter whose state is `completed`
is answered as durable even if the driver failed after that turn (`fail()` now resumes the waiters
left with a distinct `failed` outcome). The suite's `repeat_latent` case guards it, killed by the
`deferred-ignored` mutant.

- 261117-115626-2642cf `latent`, exit 0. The probe's first block now accepts a throw from
  `commit()` when the driver is failed afterward, and both the `run()` and blocking-entry blocks
  check a waiter on record 1. `commit()` throws "repeat call failed" with record 1 durable and
  `failed()` true; `run()` under a stop request throws the error, and the waiter on record 1 is
  answered as durable; the blocking entry throws the error after its `durable(1, 1)` task
  completed.
- 261117-115722-5ba8fa `crash`, exit 0: 645 cuts; on a kill turn the probe now accepts
  `commit()` throwing after acknowledging, and recovery equals the acknowledged records, the
  in-flight turn whole or absent.

Throwing from `commit()` is acceptable: `commit()` and `completed()` already throw for a failed
barrier or reclamation, and the throw now means "the driver failed", with `failed()` true, not
"this turn is unwritten". The docs (turn-seal.md, A failing repeat call) say so.

The contributor's note is right: a waiter that `completed()` claimed was made durable by that
turn, so answering it as durable matches what replay sees (crash probe); answering it with the
failure would report a written record as lost. Only a `completed` claim skips the rethrow, and
group waiters reach it only in `completed()` when their sequence is durable; every waiter left
linked gets `failed` and the error.

F1 is closed. F2 and F3 stay non-blocking notes (13592a1 also updates the fairness wording for F2).
