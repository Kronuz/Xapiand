# turn-seal review: durability

Commit: journal:b08212fe7ef5d5c976bc0429f0c5d72928fbb89a
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- build-test 261117-020559-d9967e (RelWithDebInfo, every ctest case, exit 0): runs
  `journal_turn_seal_crash` (12 seeds, power cut at every I/O operation, recovery equals the
  acknowledged model with the in-flight turn, sealed records included, whole or absent), `budget`,
  `full_size`, `bound`, `sequence`, `supersede`, `unseal_owner`, `lock_order`. Enough for A1 (a
  sealed record shares its turn's bytes and its one `sync_data`; recovery drops a torn turn whole,
  unchanged TurnLog behavior) and for the happy paths of A3, A7, A8.
- TSan 261117-020559-c74944 (`-R turn`, exit 0): `forget_threads`, `wake_threads` (wake() under the
  record mutex while run() seals on another thread, 300 records all durable),
  `unseal_threads` (unseal racing a running sealer, 200 rounds of re-seal + unseal). No race or
  lock-order report.
- Mutants 261117-020600-2e3287 (sequence check dropped, `sequence` fails) and
  261117-020603-a0abbc (checkpoint guard dropped, `supersede` fails): A2's and A7's checks are live.
  Their build step applies `test/turn_seal_mutants.sh`, which refuses a stale anchor.
- None of A1-A3, A7, A8 is tagged `--for` yet; the jobs above are the evidence to tag.

## What I checked myself

Probe job 261117-021513-beae46 (exit 1: every probe reproduces), from
`.scratch/review-turn-seal-durability/` in the home checkout (`run.sh`, `probes.cc`). Rerun it at a
fix commit: `campaign job submit --quest turn-seal --repo ~/code/Kronuz/journal -- sh
~/code/Kronuz/Xapiand/.scratch/review-turn-seal-durability/run.sh`; it exits 0 once every probe
holds.

- `quiet_block`, `quiet_run`: a waiter's open record that fits an empty turn but not the room left
  behind a staged record of another group.
- `forget_unseal`, `reseal_unseal`: unseal from another thread while the sealer, having removed its
  own registration, keeps running.
- `woken_uaf` (ASan): a wake() posted as run() ends, the driver destroyed on the loop after run()
  returns.
- By reading: lock nesting of `sealers_mutex_` and `loop_mutex_`; the sealer is called with no
  driver lock held (`turn_driver.h` seal_turn); every `TurnRecord` checkpoint constructor in
  journal, bench, fuzz and tests; external users of `TurnRecordKind` (consensus, strata, Detent,
  prospect, cluster: none in code).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A sealer called first in a turn is marked quiet when it declines, even when the room it got was less than the whole segment because staged records took some (`turn_driver.h:358`: `if (first) quiet = true`). Its owed waiter is then never answered: the blocking entry throws `stalled`, run() sleeps with the waiter linked, and the record is never offered the whole room. This breaks A3 ("refused for budget is called again on the next turn") exactly in R3's case, a large record behind any staged record. Probe: room 13396 of 13661, group 1 durable through 0, `stalled`. | 261117-021513-beae46 `quiet_block`, `quiet_run` | Mark quiet only when the sealer was offered the whole room (no staged record and first); otherwise set `resume_at`, as for a declined share. Test: `budget` case with a staged record ahead of a first-called sealer and a waiter on its record; the record is durable after two turns, under both the blocking entry and run(). |
| F2 | yes | defect | `wake()` posts `woken(this)` to run()'s loop and nothing retracts it: when run() returns (stop) with that call still queued and the owner destroys the driver on the loop, the queued call runs on freed memory. ASan: heap-use-after-free in `TurnDriver::woken` (`turn_driver.h:478`). In the threaded shape a writer's wake() racing the loop's close does the same. | 261117-021513-beae46 `woken_uaf` | Make a queued wake outlive nothing: e.g. count posted wakes and have run() not return until they ran, or post a small shared state the driver detaches under `loop_mutex_` in `~Loop`. Test: the `woken_uaf` shape as a suite case (sealer wakes and stop is requested in the same pass, driver destroyed after run() returns, loop drained), under a sanitizer job or with a detach flag it can check. |
| F3 | yes | defect | `unseal(group, owner)` waits only when the table still maps the group to that owner's sealer. If the running sealer has forgotten its own group (`forget` from a sealer is a tested path, `forget_from_sealer`) or re-registered it for another owner, an unseal by the original owner from another thread returns at once while its sealer is still running, so the owner's destructor can free what the sealer uses. The header promises "returns once that sealer is not running". | 261117-021513-beae46 `forget_unseal`, `reseal_unseal` | Record the running call's group and owner beside `running_`, and in unseal wait while that (group, owner) is running, whether or not the table still holds it. Test: both probe shapes in `unseal_threads`. |
| N1 | no | defect | A Sealer is destroyed under `sealers_mutex_`: unseal's local `fn` is declared after `hold` and dies first (`turn_driver.h:162-165`); forget's `drop` and seal()'s replacement erase under the lock. A capture whose destructor calls `sealed()` or `unseal()` self-deadlocks. Not probed. | `turn_driver.h:120-123,145-149,158-165` | Move the dropped `shared_ptr<Sealer>` out and release it after unlocking. |
| N2 | no | gap | `wake()` ignores `post_call`'s false return (a scheduler without post_call: a sleeping run() is never woken), and "any thread" holds only for a scheduler whose post_call is thread-safe (`inline_scheduler`'s is a plain push). | `turn_driver.h:170-174`; task.hh:222, 399 | Say so in the contract, or fail loudly when nothing was queued. |
| N3 | no | nit | `covers` is not mandatory at compile time (defaults to 0), but the guard fails closed: any later checkpoint without it is refused (tested). On the Journal path covers is `record_sequence_` by construction, so the shared guard cannot fire there; Journal's safety rests on `writable()` refusing a pending mutation (unchanged). Bench, fuzz, placement and power tests pass `sequence - 1` and never exercise a refusal. A covers above `sequence - 1` is admitted. No acknowledged record can be dropped through TurnLog unless a caller overstates covers. | `turn_log.h:253-255`, `journal.h:447-449` | None needed; optionally refuse `covers >= sequence`. |
| N4 | no | gap | If staged records leave less than `record_cost(0)` of the whole room every turn, no sealer is called at all (`break` with `resume_at`), indefinitely. Needs segment-sized turns of staged records; unlikely at the 64 MiB default. Not probed. | `turn_driver.h:343-345` | Note it in the mechanism doc, or reserve the first record's room for a sealer owed past N turns. |
| N5 | no | nit | A sealer calling `commit()` or `begin()` is not refused (`sealing_` guards only `stage()`); a nested `seal_turn` would call the running sealer again in the same turn. Contract misuse; not probed. | `turn_driver.h:183, 225-231, 333` | Refuse them while `sealing_`, as stage() does. |

## Not run

- No new crash model: A1's existing crash test (build-test d9967e) covers sealed records under power
  loss; I did not add torn-write cases beyond what `jt::Disk` models.
- No local TSan build: the probes' threading is timing-forced (yield and sleep), run plain; F3's
  probe is deterministic in practice but not a TSan result.
- Linux and FreeBSD: CI's. No CI run exists at this commit.
- strata's use of the API (its shim at strata:efe25bcd predates this API); Detent's lead reviews the
  shape.
