# wbuf-inline: journal

State: ready for review
Quest: .campaign/xapiand/quests/wbuf-inline.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Final head strata:3df7af4, rebased on strata main strata:446410f (main's only change since
strata:70df28d is `.github/workflows/ci.yml`). Not pushed: the lead pushes, runs build-test and
macOS asan and tsan at it (A5), CI's `linux-asan-ubsan` and `linux-tsan` (A6), and launches the
correctness review. Since strata:efc5663 only docs and scripts changed: strata:3946492 (was
strata:bb2f58d) changes `docs/perf/wbuf-inline/landed-ab.sh`; strata:3df7af4 adds the landing
A/B to `docs/perf/wbuf-inline.md` and its raw files.

## Next

1. The lead: push strata:3df7af4, the gate jobs at it, the correctness review (scope
   strata:7791520..3df7af4 per the quest's Decisions).

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | At strata:3df7af4: `test/wbuf_mutants.sh`, job 261109-091112-ef7670 ("all mutants killed"): m1 on `long_key_records` alone (rc 1, a write buffer record past a chunk), on the whole `write_buffer_test` and on `record_strides`; m2 under ASan+UBSan on `long_key_records` (heap-buffer-overflow). `long_key_records` and `record_strides` pass in build-test 261109-075141-87a30b (74 of 74). |
| A2 | At strata:3df7af4: mutants 261109-091112-ef7670, build-test 261109-075141-87a30b. Job 261109-012437-13d8c1: m4 (every layout inline) does not compile `fields_test` (`add_stored_view would skip a plugin's own add`), m5 (m4 without both `static_assert`s) fails `own_builder_parts` at `fields_test.cc`; `fields_test` in build-test 261109-012436-323159. |
| A3 | At strata:3df7af4: build-test 261109-075141-87a30b, asan 261109-075208-1546f8, tsan 261109-075208-b1cedd. `lane_charged` (`test/write_buffer_reads_test.cc`) passes in build-test 261109-012436-323159, asan 261109-012436-24bfa7, tsan 261109-012436-43f2fe (74 of 74 each, strata:efc5663). |
| A4 | At strata:3df7af4: mutants 261109-091112-ef7670, build-test 261109-075141-87a30b. Job 261109-012437-13d8c1: R (doubling chunks restored) killed by `lane_growth`, `rejection_beside_readers`' peak bound, `pressure_flush_bounded` and `defer_io_test`'s identity (digest `0x6cc0ff118bf776d3`); M1 (a 16th) by `lane_growth`; M2 (`room_for` off the lane's sizes) by `chunk_limit_room`'s `CHECK(exact)`; M3 (1 KiB chunks) aborts (`chunk directory is full`). The tests pass in build-test 261109-012436-323159. |
| A5 | At strata:3df7af4: build-test 261109-075141-87a30b, asan 261109-075208-1546f8, tsan 261109-075208-b1cedd. macOS at strata:efc5663: build-test 261109-012436-323159, asan 261109-012436-24bfa7, tsan 261109-012436-43f2fe, 74 of 74 each. At the final head strata:3df7af4: build-test and macOS asan and tsan are the lead's (code identical to strata:efc5663 apart from docs and scripts). |
| A6 | `ci:linux-asan-ubsan` and `ci:linux-tsan` at strata:3df7af4. `resource_budget`'s 1-in-300 main flake: ledger, chunk round. |
| A7 | At strata:3df7af4: 261109-091112-7d1b55. `test/prospect_integration.sh`, job 261109-012439-4268e8 at strata:efc5663 with `--ref prospect=…/prospect@test/wbuf-inline-flushes` (prospect:c0c4260) and `--ref main=main`: prospect ctest 51 of 51; churn and dump identical to prospect on strata main at flush threads 1 and 2 (132 and 2 churn lines, 36 dump lines). Also green at strata:e4fe8f7: 261108-230852-0806c9. |
| A8 | At strata:3df7af4 against main: build 261109-091112-c92d63, share sweep 261109-091112-327bc3 (records per flush 1.07-1.67x main's at all 21 shares, digests identical), wbufrr strict job 261109-091112-1c1faf. `pressure_flush_bounded`'s 8 segments: build-test 261109-075141-87a30b. `docs/perf/wbuf-inline.md`, "Landing A/B". |
| A9 | `docs/perf/wbuf-inline.md` (landing A/B at strata:3df7af4). |
| A10 | `docs/mechanisms/write-buffer.md` ("Design vs code"). |
| A11 | At strata:3df7af4: 261109-091112-63153d, `write_buffer_test` x300 on the FreeBSD VM, 0 of 300 lost. Cause: two spin-bounded waits in `test/write_buffer_test.cc` (`checkout_checkin`, `checkin_fences`) outlast the merge on a loaded runner, on main since strata:7b8ebe5; fixed by `wait_merges_done` (60 s by time). Main comparison: see the note below the table. |

History behind the rows (not evidence at the landed code): mutants at strata:efc5663 261109-012437-13d8c1 (the earlier 261108-230851-c6dcbb was invalid: m1 needed `long_key_records` alone, and M1's log overwrote m1's on the case-insensitive disk); macOS gates at strata:efc5663 261109-012436-323159, -24bfa7, -43f2fe; the local Linux VM sanitizer runs 261108-230853-7da613, 261109-012440-05ac8b, 261109-032020-5807c6 gave way to CI; A8 at strata:bb2f58d: sweep 261109-024406-5881b6, wbufrr 261109-063604-6b6841 (1.064 [1.042, 1.091], B/B 0.978 [0.959, 1.003], peak RSS 32/34 MiB), builds 261108-230854-8255bb and 261109-012441-072616 superseded, 261109-043525-e7d3ad and 261109-055012-9f1069 cancelled by the lead (host drain); A11: main at strata:5cb0e5a lost 49 of 300, all `checkout_checkin` (261108-214156-aa0cbd), the branch 0 of 300 at strata:efc5663 (261109-012438-75c07d), strata:e4fe8f7 (261108-230851-b11787) and before the rebase (261108-214155-6b7f28). Review probes 261109-081601-258c10 and 261109-084514-bff9a1 did not compile (the reviewer's probe, not this code); 261109-085204-31b428 passed under TSan at strata:3df7af4.


## Result

strata:3df7af4 on `quest/xapiand/wbuf-inline`, rebased on strata main strata:446410f. The write buffer
keeps a record's key and stored bytes inline for every layout that opts in, bounded by its chunk;
the lane's chunks are charged to the budget and grow by a 32nd of the lane, 4 KiB to 1 MiB after a
1 KiB first. Against main, measured at strata:bb2f58d (same code): records per flush 1.07-1.67x main's,
digests identical (job 261109-024406-5881b6); readrandom 100 B 1.06x faster [1.042, 1.091], B/B a
tie (job 261109-063604-6b6841), against the quest's "a tie". The FreeBSD `write_buffer` flake
(pre-existing, 49 of 300 on main) is fixed: 0 of 300. Mutants m1, m2, m4, m5, R, M1 to M3 all
killed by `test/wbuf_mutants.sh`. prospect's suite passes with its companion branch, output
identical to strata main.

This round added the job scripts `test/wbuf_mutants.sh`, `test/prospect_integration.sh`,
`test/prospect_churn_gen.py`, `test/linux_asan.sh` and `docs/perf/wbuf-inline/landed-ab.sh`, and
`write_buffer_test NAME` (runs one test; an unknown name fails). Docs: `docs/perf/wbuf-inline.md`
("Landing A/B", raw `docs/perf/wbuf-inline/landed-*`), `docs/mechanisms/write-buffer.md`.

Open:
- prospect's repin to this commit (quest wbuf-inline-prospect).
- Values past about 990 B and layouts carrying more than the stored bytes keep whole records
  (the quest's Decisions, architect).
- `test/linux_asan.sh` is unused while CI covers Linux.
- The slot `xapiand-wbuf-inline-ab` keeps `.scratch/ab/` (the four binaries, and `build-A` from the
  failed build 261108-230854-8255bb); removing it with the quest's cleanup is the lead's.

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
- `readrandom` is 1.06x faster than current main where the chunk round measured a tie against
  strata:9e344d8. Recorded as measured, not as a target change: main moved 86 commits between the
  two baselines.
