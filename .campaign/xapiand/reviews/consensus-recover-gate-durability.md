# consensus-recover-gate review: durability

Commit: consensus:b6a604a (b6a604abf39409b8db000e7c77977c529bcc0db5)
Verdict: APPROVE
Date: 2026-11-25

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261125-034822-6f0c97: the four `recover_gate_*` cases pass at b6a604a (47, 35, 11, 53 checks, 0
  failures).
- 261125-034825-3c9017 (`--fails-without`, mutant `gate-after-start`): lone fails only at the
  state-change checks: term 2/3/4 on the row, "the Sink hears nothing", byte equality on each of
  the three attempts, and the final "leads the next term". Follower fails only at "no vote
  persisted: term 5". No crash, no unrelated failure; the base/last/reason checks still pass,
  so the mutant dies on what the gate protects, not on incidental text.
- 261125-034814-cbfb1c: build-test, 179 of 179 tests pass. CI: `campaign status` lists no CI item
  left to land at b6a604a.

## What I checked myself

Probe job 261125-040440-f9dd6c, scripts in
`.scratch/xapiand/review-consensus-recover-gate-durability/` (`run.sh` inserts `probe.cc` into
driver_test.cc and runs it; 0 check failures):

- `probe_closed_restart`: a lone voter closed into the host's table, refused four times, each a
  process restart. Attempt 1 moves the table entry closed (2) to open (1) and changes the files
  (journal.h:604-610 at journal:f66c16e, the D7 write); attempts 2-4 leave the files equal to an
  idle restart's. `closed_state` on the refused row is none. Accepted at its base afterwards, the
  kept closed record still resumes its term with no election (term and last unchanged).
- `probe_refused_inbox_wakes`: a refused follower row left open (N-1).

Code reading, by question:

1. Nothing consensus-visible persists or sends before the gate, by construction. The gate runs
   synchronously in `Driver::recover` (driver.h:372) after `Worker::recover` and `Worker::reopen`,
   before any turn. `Core::reopen` only sets `resume_`/`term_record_` in memory (core.h:320-327).
   Bootstrap-only typed init (`initialization_`, worker.h:178) and the pre-Start reclaim
   (worker.h:502-507) run only inside `run_one`, whose `turn` returns `Fenced` at worker.h:514
   before inventory, init or reclaim; `ready()` is false when fenced, so `admit` never submits
   Tick + Start (driver.h:921-924). `close_store` returns when fenced (worker.h:691): no close
   record. The only write is the journal's reopen of a closed entry, documented, and stable after
   the first attempt (probe).
2. `retains_suffix` (core.h:329) is D2's `holds(H)`: base equal (index and term), inside the log
   at the same term, or refused (below base, past last, other term at an index, other term at the
   base index). After an install's reset the bundle boundary is the base, so it is accepted;
   (0, 0) is accepted only by a base-0 log. Tests cover base, inside, one of several, empty,
   below, past and other-term; the install case is by construction only.
3. The refused row is `opened` and fenced; `report_fence` is silenced by `fence_seen`
   (driver.h:661, 788), so no `Fenced` to the Sink and no `Gone`. A retry without close fails
   `fenced` with the reason (begin(), driver.h:540); close retires it through the fenced path
   and erases the row; retries after close in the live host and across restarts find the same
   base, last and terms (tests and probe). No hang; one resource issue (N-1).
4. `Worker::recover` is untouched (worker.h adds only the `retains_suffix` accessor); Detent's
   callers in Detent/ use the Worker or their own types. The old `Driver::recover` overloads
   forward an empty `accept` (driver.h:355). No caller passes a bare `{}` fourth argument that
   would now be ambiguous (strata group.hh:263, witness.hh:47).
5. Tests compare values: base, last, term, reason text, membership, committed terms by index,
   byte equality against an idle-restart control. The mutant fails only at its named checks.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N-1 | no | defect | A refused row left open with a message in its inbox wakes every 1 ms until closed: `admit` returns before draining the inbox when not started (driver.h:921), the fenced Worker has no deadline, and `arm`'s `waiting` branch re-arms at `retry_ns` (driver.h:870-871). Without an inbox message it rests. The same path already exists for the `over_headroom` fence before Start; the gate makes it a designed resting state. strata closes in its open path, so today the spin is short. | 261125-040440-f9dd6c: 2000 handles in 1000 ms with one VoteRequest queued, 0 without | Lead's call. Drop a fenced group's inbox and notices (or ignore them in `arm`'s `waiting`) when the Worker is fenced. Test: refused follower with a queued message, advance 1 s, `armed_timers() == 0`. |
| N-2 | no | gap | `recover_gate_closed` retries only in one live host and never checks the table status or bytes; the closed-to-open transition at the first refusal and its stability afterwards are shown only by the probe. | 261125-040440-f9dd6c (`probe_closed_restart`) | Fold `probe_closed_restart` into the suite as a value test. |
| N-3 | no | nit | The journal's lifecycle paragraph says `opened` stays false on refusal; the code sets it (driver.h:373) and the journal's own Rulings say so. | journal "Lifecycle: recover to Start" vs driver.h:373 | Correct the paragraph. |
| N-4 | no | nit | For the consumer: `closed_state` on a refused row is none. A host that overwrites its kept closed record with none at close loses the record, so the next accepted open costs an election. | 261125-040440-f9dd6c | strata-journal-behind keeps its prior closed record when closing a refused row. |
| N-5 | no | nit | Under the mutant, the follower's "answers no vote" check passes: the vote persists (term 5) but the fence lands before the reply goes out. The term check carries the case. | 261125-034825-3c9017 log | None needed. |

## Not run

No bootstrap-only (typed init queued) or above-headroom group refused at the gate in a run: both
are argued from worker.h:502-514 only. No install-reset log through the gate in a run. No crash
model (faultfs power cut) between reopen and the refused close. No sanitizer or other-platform
run beyond CI. Session title not checked (subagent).
