# wbuf-inline: journal

State: waiting on jobs 261108-230850-b72759 261108-230850-ae766d 261108-230850-119b9c 261108-230851-b11787 261108-230851-c6dcbb 261108-230852-0806c9 261108-230853-7da613 261108-230854-8255bb 261108-230855-fae46b 261108-230856-f04186
Quest: .campaign/xapiand/quests/wbuf-inline.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto strata main (strata:70df28d), clean, no conflicts (main touched `index.hh` in two
hunks the branch does not). Head strata:e4fe8f7 adds four job scripts, no engine change:
`test/wbuf_mutants.sh` (A1, A2, A4), `test/prospect_integration.sh` with
`test/prospect_churn_gen.py` (A7), `test/linux_asan.sh` (A6), `docs/perf/wbuf-inline/landed-ab.sh` (A8).
Jobs at strata:e4fe8f7: build-test 261108-230850-b72759, asan 261108-230850-ae766d, tsan
261108-230850-119b9c, A11 261108-230851-b11787, mutants 261108-230851-c6dcbb, prospect
261108-230852-0806c9, Linux ASan 261108-230853-7da613, A/B build 261108-230854-8255bb, share sweep
261108-230855-fae46b, wbufrr 261108-230856-f04186.

## Next

1. (Waited once, 600 s: tsan running, the rest queued behind the host's light slots.) Read the jobs at strata:e4fe8f7; fill A1 to A11 in Evidence; add the landing A/B to
   `docs/perf/wbuf-inline.md` (a docs-only commit; then rerun `campaign verify` at that head, the
   other jobs' code is unchanged).
2. Ready for the correctness review.

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A11 | Explained: two spin-bounded waits in `test/write_buffer_test.cc` (`checkout_checkin`, 20,000 yields; `checkin_fences`, 200,000) outlast the merge on a loaded runner, so `merges_done` stays false; both on main since strata:7b8ebe5. Fixed by `wait_merges_done` (60 s by time) at strata:47628b2 and strata:8f34321. Branch 0 of 300 failed, 4 at once on the FreeBSD VM: job 261108-214155-6b7f28 at strata:8f34321. Main 49 of 300 failed, all at `:467` (`checkout_checkin`): job 261108-214156-aa0cbd at strata:5cb0e5a. Earlier attempt: 51 of 300 with only the fence wait fixed (261108-212517-0c65b3); 261108-212154-247abd, 261108-212155-96e977 and 261108-212518-73fa81 never ran (the VM key, then an ssh timeout under load). Rerun at the rebased head. |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- The lead: A11's repeat harness is `test/vm_repeat.sh` (strata:c15019c, strata:287b461); it drives strata's FreeBSD VM from the main checkout (STRATA), like cpp-libraries' preflight.
- Rebase onto strata main kept main's behavior: no conflict; `index.hh`'s main hunks
  (strata:fd2ed66, strata:f70fb2c) apply beside the branch's.
- The share sweep (A8's records per flush) runs as a light job: its counts and digests are
  deterministic; max RSS from it is noisy and not claimed. The wbufrr timing runs `strict`.
- A6's Linux ASan+UBSan runs on strata's local Linux VM (port 18772, no AWS) through the main
  checkout's `tools/vm/aws-linux.sh run-local`, as A11 drives the FreeBSD VM; CI on the landed
  commit stays the lead's.
- Superseded jobs: 261108-225901-3a3729 (build-test at strata:14ed454, the rebase before the
  scripts: passed, ctest 74 of 74; same C++ as strata:e4fe8f7) and those cancelled at strata:14ed454, strata:502b330 and strata:abea21e (the head moved).
