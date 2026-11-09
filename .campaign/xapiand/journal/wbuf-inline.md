# wbuf-inline: journal

State: waiting on jobs 261109-043525-e7d3ad
Quest: .campaign/xapiand/quests/wbuf-inline.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata:efc5663 (rebased on strata main strata:70df28d). Fixes after the lead's message on the
jobs at strata:e4fe8f7:
- Mutants 261108-230851-c6dcbb reported "m1 write_buffer_test: SURVIVED (rc=134)". The pattern was
  wrong, the test was not: in the whole binary m1 aborts an earlier test on the same uncaught
  `logic_error` ("past a chunk") before `long_key_records` runs. The log the lead read was M1's:
  `M1-write_buffer_test.log` overwrote `m1-write_buffer_test.log` on the case-insensitive disk,
  which is where :691 and :723 came from. A probe build (m1, Release, under `quiet light`) showed
  `write_buffer_test long_key_records` alone: rc 1, `long key cap: strata: a write buffer record
  past a chunk`, 5 failures; whole binary: rc 134, `terminating due to uncaught exception of type
  std::logic_error`. Fix: `write_buffer_test` runs one named test (an unknown name fails, rc 2);
  the script runs m1 and m2 on `long_key_records` alone (the review's claim) plus m1 on the whole
  binary, and numbers its logs.
- A8 build 261108-230854-8255bb: `kv_proto_bufmem` exists only with `STRATA_KV_PROTO=ON` on both
  sides. `landed-ab.sh` compiles flush_cadence with kv_strata's commands instead (dry-run of the
  substitutions on strata main's build dir).

Jobs at strata:efc5663: build-test 261109-012436-323159, asan 261109-012436-24bfa7, tsan
261109-012436-43f2fe, mutants 261109-012437-13d8c1, A11 261109-012438-75c07d, A7
261109-012439-4268e8, A6 261109-012440-05ac8b (after 261108-230853-7da613, so the VM runs one at
a time), A/B build 261109-012441-072616, share sweep 261109-012442-0cf880, wbufrr 261109-012442-7003e7.
Green at strata:e4fe8f7 (same engine code; e4fe8f7..efc5663 changes only `test/write_buffer_test.cc`'s
main and the scripts): tsan 261108-230850-119b9c, asan 261108-230850-ae766d, build-test
261108-230850-b72759, A11 261108-230851-b11787, A7 261108-230852-0806c9; A6 261108-230853-7da613 running.

Passed at strata:efc5663 (the lead): build-test 261109-012436-323159, asan 261109-012436-24bfa7,
tsan 261109-012436-43f2fe, mutants 261109-012437-13d8c1, A11 261109-012438-75c07d, A7
261109-012439-4268e8. A6 261109-012440-05ac8b is still running.
A8 build 261109-012441-072616 failed: a build directory in the shared slot `xapiand-wbuf-inline-ab`
was configured from the previous job's `ref-main`. strata:bb2f58d keys each arm's build directory
by its commit, drops a CMakeCache from another source path, and removes only its own directory.
That commit changes only `docs/perf/wbuf-inline/landed-ab.sh`. Resubmitted at strata:bb2f58d: build
261109-024405-35d764, share sweep 261109-024406-5881b6, wbufrr (strict) 261109-024407-5fdab0. The gate
checks I queued there (261109-024404-c71a6a, -55cb2e, -0b7c81) are cancelled: verify runs once at
the final head, after the perf note's docs commit.

The lead cancelled the strict A/B 261109-024407-5fdab0 and the Linux VM A6 261109-032020-5807c6
to free the host: CI's `linux-asan-ubsan` covers A6. The strict wbufrr A/B was resubmitted at
strata:bb2f58d as 261109-043525-e7d3ad. Share sweep 261109-024406-5881b6: B/A records per flush
1.07-1.67x, digests identical at all 21 shares, counts equal to the ledger's A and G columns.

## Next

1. (Waited once, 600 s: 261109-043525-e7d3ad running under its strict hold.) Read wbufrr 261109-043525-e7d3ad; write the landing A/B into `docs/perf/wbuf-inline.md`; rebase on strata main (CI trigger); give the lead the head; build-test plus macOS asan and tsan at it; A6 = `ci:linux-asan-ubsan`, `ci:linux-tsan`; fill A1 to A11 in Evidence; add the landing A/B to
   `docs/perf/wbuf-inline.md` (a docs-only commit; then rerun `campaign verify` at that head).
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
- `write_buffer_test NAME` runs one test: the smallest change that lets a mutant be checked
  against the test the review named, not just against the binary.
- Gate checks run once more at the final head (after the docs-only perf-note commit), not at each
  script fix: strata:efc5663..bb2f58d changes only `landed-ab.sh`.
