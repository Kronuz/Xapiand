# wbuf-inline: journal

State: working
Quest: .campaign/xapiand/quests/wbuf-inline.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A11 is done at strata:8f34321 (the lead's work, 2026-11-08). Next: rebase onto strata main.

## Next

1. Rebase quest/xapiand/wbuf-inline onto strata main (162 files changed since the branch point), then
   `campaign verify wbuf-inline` and resubmit the A11 repeat at the rebased head:
   `campaign job submit --quest wbuf-inline --level light --minutes 120 -- test/vm_repeat.sh write_buffer_test 300 4 branch=.`
2. Fill A1 to A10 from the branch's ledger (strata `docs/status/wbuf-inline.md`, Status: HISTORY) with jobs at the rebased head.
3. Ready for the correctness review.

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
