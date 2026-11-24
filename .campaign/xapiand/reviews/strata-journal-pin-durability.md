# strata-journal-pin review: durability

Commit: strata:1cbb6e309217952db5acfaf4e58aaf6e5fb09e35
Verdict: CHANGES
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A1 261123-194216-e06393 (build-test, asserts on), A2 261123-194218-5b493a, A5 261123-194221-21e52b,
  A6 261123-194233-72e14d: passed, as the lead reported. All ran at strata:22ee0282, whose code
  1cbb6e30 shares (1cbb6e30 changes only docs). A5 and A6 build with `-DCMAKE_CXX_FLAGS_RELEASE=-O3`,
  so asserts are on.
- Mutants killed: 261123-194337-4c3177 (bounded-as-stored), 261123-194339-539e7b (bounded-at-bound),
  261123-194341-31812e (bounded-created). strata CI 45321560305 green (per the lead).
- 261123-194339-539e7b's log is the evidence for F1. With `<` restored, t5b fails at exactly one
  CHECK, `standalone_turn_log_test.cc:1009` (`== outcome::lost`), with "1 failures". So the head-at-bound
  open took the kept path, and every `CHECK(ix.get(key) == value)` at :975 passed for every
  acknowledged write.

## What I checked myself

- Q1, read: `index.hh` join/seal/checkpoint (:493-575) and commit (:1303-1320). A commit holds the
  writer lease, waits until the log is durable through `applied.index` (which needs that record
  sealed), and `seal()` takes the whole open batch (`count = 0; open = 0`). `checkpoint()` throws
  unless `open == 0` and `x.index + 1 == next`. So a committed head is always `{s, k}` with record s
  sealed at exactly k transactions. A head `{bound, i}` with `i < k` cannot exist, and a head at the
  bound holds that record's whole batch.
- Q1, probe: `.scratch/xapiand/review-strata-journal-pin-durability/at_bound.{sh,py}` in the strata
  worktree. It patches the check to `<`, makes t5b's at-bound case expect kept, and adds a case with
  the head at bound + 1 that expects kept. Job 261123-195343-f2794e never left the queue and was
  cancelled once 539e7b's log answered the question. Rerun it on the fix.
- Q2: `git grep GroupStatus` at 1cbb6e30 over include, src and sim, plus consensus and Xapiand src.
  The only readers are `index.hh:3386-3397` (attach_log), `group.hh:255` and `witness.hh:46`. All
  three handle `bounded`. The catalog opens through attach_log. A reinstall runs only on an open
  group, and a group is `bounded` only at `Host::open`, so the reinstall path never sees it.
- Q3: journal `journal.h:597` (`recover_shared`: `host_->lose` before `Corruption`) and `host.h:411`
  (`lose` → `mark_lost` → `commit`, durable on return). A crash before that commit leaves the group
  bounded, and the next open repeats the same path. A crash after it leaves the group lost. No
  window lets the bounded records come back. group.hh's failed-open `driver.close` cannot write
  `close_group` over the loss: the Store is `failed`, so `writable()` throws. A6's lifetime
  `bounded` case checks `lost` after a host restart.
- Q4: `git diff a3eecb5 9c7e31c -- include` in journal.
  - `deleted` / `delete_group`: none on a deleted group, and `create_group` keeps the floor.
  - Damage detection (`later_turn`): it only adds bounded or lost outcomes.
  - `memory_io.h` removal: strata references it only in docs.
  - The deferred commit of create and reopen predates a3eecb5. strata's comment now says so
    (`turn_seal.hh:124`). D7's join tolerates it: a crash before the commit leaves the group none
    with the attribute set, and `join(member)` creates it again.
  - Nothing else strata must follow.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | `attach_log` loses a bounded group when the head is **at** the bound (`<=`, `index.hh:3393`), but such a head always holds that record's whole batch (see Q1). The change permanently loses the group of an index that has every acknowledged write: `lose()` is durable and refuses the id for good, so the index never opens on its log again. This contradicts journal's contract ("a reader whose state already holds the group through the bound … continues", lost-bound.md "Who may replay past it") and main's `<` (turn-log-parity, durability-reviewed). t5b's at-bound case enforces this refusal, and the `bounded-at-bound` mutant guards it. Below the bound and past it, behavior is correct. | 261123-194339-539e7b log (only :1009 fails, :975 value checks pass); `index.hh:493-575`, `:1303-1320` | Restore `<` in the check and fix the comments at `index.hh:3369-3370`, `turn_seal.hh:108`, and the `replication-group.md` bullet. t5b's at-bound case expects `kept` with every acknowledged write, and a case with the head at bound + 1 is added. Replace the `bounded-at-bound` mutant with its inverse (`<` → `<=`), which that case must kill. The probe script above is that test. |
| F2 | no | gap | The witness's `bounded` route (`witness.hh:46`) has no test. It reaches the same `Store::recover_shared`, so the risk is low. | `witness.hh:46` | A witness variant of lifetime `bounded`, when group tests next change |
| F3 | no | gap | A damaged replica has no path back. This change makes nothing worse: before the bound (9e11713), `Host::open` marked such a group lost. On main (a3eecb5), `driver.create` threw and left it bounded, and it never opened again either. Now it is lost durably, which matches the old outcome, and it fails as Corruption instead of `logic_error`. The hunt row (hunt.md:498) is the right home. | hunt.md:498; `host.h:521`, `:563` (`delete_group` needs `closed`) | Follow-up as filed. Note that a lost id is refused for good and `delete_group` refuses it, so the follow-up needs a journal entry point. |
| F4 | no | nit | `lost` still goes to `driver.create` in group.hh and witness.hh, so it fails as `logic_error` rather than Corruption. The outcome is unchanged (journal ruling 3). | `group.hh:255` | The one-word follow-up the contributor named |
| F5 | no | gap | `create_group` after `delete_group` keeps the deleted incarnation's floor. That matters only when index-delete calls it: the index's join must stage its first record above `retired`, or recovery treats it as superseded. Not this quest's call path. | journal `host.h:521-531`; `index.hh:3476` | index-delete proves it |

## Not run

- My probe job 261123-195343-f2794e (cancelled while queued). F1 rests on 539e7b's log and the code
  reading above. Rerun the probe on the fix.
- No crash sweep over the `lose()` commit inside `recover_shared`. That is journal's mechanism, and
  my check is a reading of `mark_lost`.
- Not checked on Linux or under sanitizers locally (CI's).
- Session: this review ran as a subagent, not a session titled `xapiand · reviewer strata-journal-pin durability`.
