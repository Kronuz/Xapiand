# consensus-flow-bench review: correctness

Commit: consensus:d7091f66de9d08f2d16ead6c40ac978d423f4d82
Verdict: CHANGES
Date: 2026-11-18

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Both blocking findings are doc fixes (`docs/perf/flow-control.md`); no bench code change is needed
and the three evidence jobs stay valid. Every number I spot-checked matches the raw output.

## Evidence I relied on

- A1 `261118-114928-9bef7d`, A2 `261118-114946-13cf10`, A3 `261118-135806-e23693`: all exit 0 at
  consensus:e7a453b (bench and script identical at d7091f6; the later commits are merges from main
  and the doc). I read their logs, not the doc's summary of them.
- Spot-check of the doc against those logs, all matching: A1 three-run medians and ranges for
  the 0, 1 and 10 ms 256 KiB rows (including 185/s, 125/s, 56/s, 208-212/s), the five ratios
  (1.12, 1.69, 3.70, 1.62, 1.83, 4.53), the 2 MiB rows (14%, 24%, 36% slower), the barrier lines;
  A2 rows 1 to 4 and 9 to 10 (ranges, p99s, undelivered 524288 and 786432 bytes, "28-43% less");
  A3 rows 0 ms Bulk, 0 ms Control 256 KiB, 1 ms Control 2 MiB, 10 ms 1 Gb/s Control 256 KiB, the
  Bulk summary ranges (6.5-37.4 ms delay, 14.2-38.9 ms gap, 15-122 ms bound, 2.9-4.5 and
  3.2-4.7 ratios) and the 1.6-2.0 and 16 times lane comparisons. 300 of 300 control frames in all
  24 rows; the dropped counts match "0-2".
- Method nit only: the 0 ms `pipeline = 1` latency (376 ms) is the median-throughput run's, the
  10 ms rows' (2.22 s, 23.9 ms) are the median of the three p50s (the median-throughput runs read
  2.18 s and 24.8 ms). Raw p50 at 0 ms ranges 376-940 ms: queue age, as the doc says.

## What I checked myself

Script `.scratch/xapiand/review-consensus-flow-bench-correctness/probe.sh`, job
`261118-224935-cfdbeb` (exclusive, gate VM, the same build as the evidence jobs).

- P1 `ping -c 200 -i 0.01` on `lo` under the bench's own netem settings: nominal 0 ms reads
  0.02 ms; nominal 1 ms (`delay 499us`) reads min 1.71 / avg 2.81 / max 3.85 ms; nominal 10 ms
  (`delay 5ms`) reads min 11.05 / avg 12.21 / max 13.26 ms; 1 ms with `rate 1gbit` avg 2.87 ms.
  The same overshoot is in the A3 idle rows: one-way p50 1.97 ms at "1 ms" (0.5 ms plus 0.33 ms
  base expected) and 6.65 ms at "10 ms". A1's `pipeline = 1` cost agrees: 5.4 ms a command at
  0 ms, 8.0 ms at "1 ms" (+2.6), 17.8 ms at "10 ms" (+12.4).
- P2 A/A, 4 more runs each, 2 MiB appends at 0 ms: `pipeline = 1` 66.7, 64.6, 64.5, 63.5 MB/s
  (job A1 had 62.8); `pipeline = 4` 58.1, 57.7, 53.6, 55.3 (A1 had 54.2). The "14% slower" holds
  (13% on the means).
- P3 A/A, 4 runs per row, unrated, 0 and 1 ms. Bulk 2 MiB at 0 ms: max delay 2.2-3.8 ms and gap
  12.9-13.8 ms (A3: 6.5 ms and 15.1 ms). Control 256 KiB at 0 ms: max delay 7.5-10.6 ms, gap
  12.0-17.0 ms, bound about 11.6 ms (A3: 13.6, 12.3, 11.9); at 1 ms 8.3-10.4 ms (A3 10.1; bound
  15.6). Control 2 MiB at 1 ms: max delay 13.6-16.8 ms (A3: 33.8 ms; bound about 19 ms). Control
  2 MiB at 0 ms: 9.4-16.6 ms (bound about 13.9-14.2; the 16.6 ms run is over it). One Control 256 KiB run had p50 5.6 ms against
  0.7 ms in the other seven.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The 1 and 10 ms rows are labeled "added round trip" but `netem delay` on `lo` overshoots by about 1 ms a traversal in the VM: the "1 ms" row is a 2.8 ms RTT (min 1.7), the "10 ms" row 12.2 ms. Every A1, A2 and A3 row at those labels is off, "1 ms" by 2.8 times; the A1 ratios (1.69 at "1 ms") compare against a link that is 1.8 ms slower than named. The direction of every conclusion holds. | job `261118-224935-cfdbeb` P1; A3 idle p50 1.97 and 6.65 ms; `docs/perf/flow-control.md` lines 5, 134 and the A1 table | Label the rows with the measured RTT (0.02, 2.8, 12.2 ms) or say "nominal" and state the measured value beside the netem setting; a `ping` line in `flow_bench_vm.sh`'s `net` would record it per run. Do not change the bench rows. |
| F2 | yes | defect | The A3 doc misframes which rows test the stated bound. The transport states it for data frames sharing the control connection ("a control frame waits behind at most one data frame, the last feed and what the kernel's socket buffers hold", `host-transport.md` "Send"; the reconciliation's A5 row repeats it). Bulk is a separate TCP connection per lane (`links.h` slots by node and lane), so the Bulk rows measure cross-connection interference (loop CPU, TLS, the shared netem rate), not that bound; the Control rows, with 256 KiB frames R19's own case, are called "the foil" and "a bound the Control lane does not claim for data" (doc line 180), which contradicts that page. Stated as the result, "behind Bulk... within the bound" is true but is not evidence of R19's mechanism. Against that bound the Control rows read: 0 ms, 256 KiB, max delay 13.6 ms over 11.9 ms; 1 ms 2 MiB 33.8 ms over 19.0 ms. Repeats (P3) do not reproduce either: 7.5-10.6 ms and 13.6-16.8 ms, so those two were single tail events and the doc's "past its bound" sentence overstates, while one of eight Control 256 KiB runs shows a 5.6 ms p50. | `host-transport.md:136-138`; `links.h:478`; A3 log rows; job `261118-224935-cfdbeb` P3 | State that Bulk is a separate connection and what the Bulk rows therefore show; report the Control-lane rows (256 KiB first) against the bound as R19's rows, with the repeat ranges (4 more runs each, P3) beside the single run; drop "does not claim" or cite where the lane's bound is disclaimed; say whether R19's bound holds on the Control lane (256 KiB: at 0 ms in 4 of 5 runs, at 1 ms in 5 of 5; 2 MiB at 0 ms in 4 of 5 runs, the 16.6 ms probe over about 13.9 ms; always at 1 Gb/s and at 10 ms). The lead's quest said "behind Bulk traffic"; if the lead means Bulk deliberately, add one line saying so. |
| F3 | no | gap | The "longest gap" column, which includes the 10 ms cadence, sits beside the stated bound and reads 99% of it for the 0 ms Bulk row (15.1 of 15.2 ms). It can only overstate the wait (by up to the cadence), so it cannot hide a violation, but the row's real margin is the delay: 6.5 ms in A3 and 2.2-3.8 ms in four repeats (gap 12.9-13.8 ms), well under 43% of the bound. The doc explains this in prose; the table invites the wrong read. | A3 log; P3 | Compare max delay to the bound in the table and keep gap as the cadence-inclusive liveness view, or subtract the cadence. Give the repeat range for any row the prose leans on. |
| F4 | no | gap | A2's slow arm cannot reach the transport's bound: `Flow{4, 4 MiB, 256 KiB}` caps group 0 at 1 MiB in flight and the slowed barrier never blocks the receiver's loop (the inbox absorbs, no drop), so the 4 MiB window plus 8 MiB queue is never approached ("undelivered" peaks at 768 KiB, 1 MiB with fdatasync). The row shows no head-of-line block from a slow barrier on the shared connection (supported); it does not test a stalled reader. The 2 ms sampling can miss peaks. | `bench/flow_bench.cc` `deliver`, `many`; A2 log | One sentence in the doc scoping the claim to a slow barrier, with a stalled reader left to `connection_test` `slow_reader` and `transport` `upload`. |
| F5 | no | gap | No test or assertion backs any row: `test/flow_bench*` does not exist, the VM script exits 0 whatever it measures, so A1 to A3 "met" means the bench ran, and the within-bound judgments (A2, A3) are the doc's reading. Counts are checked against values nowhere: committed bytes are summed from `Committed` events at each node, not compared with the proposed payloads. Acceptable for a measurement quest; the lead should know "met" does not gate the claims. | `flow_bench_vm.sh`; `campaign status` | Optional: a nonzero exit when 300 control frames do not arrive or a refusal counter is nonzero. |
| F6 | no | gap | Single-run rows the doc leans on: the fdatasync rows (A1 `fdatasync` 4.53x; A2 fdatasync group 0 leader commit 36.1 vs 50.5 MB/s between arms, which moves 40% unexplained) and the 1 and 10 ms 2 MiB rows. The 0 ms 2 MiB row holds (P2). The 256 KiB `pipeline = 1` 0 ms row spreads 12% (43.9-49.4), so its 1.12x is the weakest ratio, though it stays above 1.10 end to end. | A1, A2 logs; P2 | Say "once" in the table cells, as the A2 table already does for fdatasync. |
| F7 | no | nit | "The storage thread busy throughout" for `pipeline = 4`: its leader's barriers (about 1,270 at about 1.8 ms) are 71-76% of the 3 s; the two-barriers-per-command explanation fits the 54 vs 112 MB/s change with the barrier (modeled vs fdatasync) but the thread is not saturated. | A1 log node 1 storage lines | "About three quarters busy". |
| F8 | no | nit | Idle rows' tail "3 ms at most" is 8.3 ms at 10 ms (baseline 6.6 ms). The VM's fdatasync is a file on APFS under `cache=none`, so it is not a device-flush rate; the doc names the disk but not that. | A3 log; `vm.sh` | Reword; one clause on the disk. |
| F9 | no | nit | Track C T7's own text sets gates (pipelined at least 4x at 10 ms; other groups' commit p99 within 1.5x) that the quest did not carry. At the modeled barrier the 10 ms ratio is 3.70x (4.53x with fdatasync); the doc reports ratios without saying which gate applies. | strata `docs/design/track-c.md` T7 | The lead decides whether T7's gate text or the quest's criteria rule; the doc can quote both. |

## Not run

- The bench on macOS or an arm64 host; only the Linux VM rows.
- A second `fdatasync` A/A for A1/A2, and 1 and 10 ms repeats of the 2 MiB rows.
- A stalled-reader variant of A2 (F4); a TSan/ASan pass of `flow_bench`.
- CI on d7091f6 (running when I started; not part of this lens).

## Re-check

Commit: consensus:d9605f40f84c306e9e14e30c5ec66e3aac11b858
Verdict: CHANGES (one blocking item left, a doc sentence)
Date: 2026-11-18

The diff from d7091f6 is `docs/perf/flow-control.md` only, so the evidence jobs and the bench stand.
I re-read the doc's changed tables and prose against the A3 log and my P1 and P3 output.

| ID | Status | Re-check |
|---|---|---|
| F1 | fixed | Rows are named 0, 2.8 and 12.2 ms in A1, A2 and A3, and the new paragraph gives the ping figures (0.02; 2.8, min 1.7; 12.2, min 11.1, max 13.3) and the idle one-way p50s (0.33, 1.97, 6.65 ms), all matching job `261118-224935-cfdbeb` and the A3 log. The intro still says "1 and 10 ms" but marks them nominal with the measured values. |
| F2 | partly fixed, blocking | Done and correct: Control rows are R19's case, listed first and set against the bound; Bulk is framed as a separate connection (cross-connection interference). The A3 table's bound and gap columns were swapped and I checked six rows: they are right. The five-of-six single-run count and the repeat table match P3 (256 KiB: 4 of 5 at 0 ms, 5 of 5 at 2.8 ms; 2 MiB: 4 of 5 at both, the 16.6 ms run over). The "past its bound" claim is now stated as a tail event. Missing: the F9 ruling. The doc names no T7 gate (grep for "gate", "at least 4" and "1.5" finds nothing), so it never says that 3.70× at 12.2 ms is below T7's 4× gate and that 4.53× with fdatasync is above it. |
| F3 | fixed | Bound sits before the gap; delay is the headline; the Bulk paragraph says the 15.1 ms gap is the cadence plus the wait, with the 2.2-3.8 ms repeat range. |
| F4 | fixed | Scoped to a slow barrier, with the stalled reader pointed to `connection_test` `slow_reader` and `upload`; the 2 ms sampling caveat is stated. |
| F5 | no change | A non-blocking gap; the lead may keep it. |
| F6, F7, F8 | fixed | Single runs are named (2 MiB rows, fdatasync rows, 4.53×, the 12% spread); "about three quarters busy"; idle tail 8.3 ms; disk caveat added. |
| F9 | open, folded into F2 | Per the lead's ruling the criteria govern this quest, but the doc must name T7's gates beside the ratios. Not yet in the doc. |

Blocking, one: F2/F9. Fix: in A1, put T7's gate (pipelined at least 4× `pipeline = 1` at 10 ms) beside the ratios and say plainly that 3.70× is below it and 4.53× with fdatasync is above it, noting the gate is judged in track-c-gates; likewise one line in A2 on T7's other gate (other groups' p99 within 1.5× the unslowed run), which the A2 numbers meet (node 2 apply p99 35.7-36.7 ms against 37.3-52.0 ms at 2.8 ms; 49.8-50.3 against 49.8-51.1 ms at 12.2 ms). Also note the gate was stated at 10 ms and the row is 12.2 ms. A doc edit only; no re-run.

## Second re-check

Commit: consensus:122564d61c2f8cd5ac1273a9c5746c7fc2419b8e
Verdict: APPROVE
Date: 2026-11-18

The diff from d9605f4 is 11 lines of `docs/perf/flow-control.md`; the bench and jobs are unchanged.

- A1: the doc now names T7's gate (pipelined at least 4× `pipeline = 1` at 10 ms), says the row is
  12.2 ms measured, that 3.70× (modeled barrier) is below the gate and 4.53× (`fdatasync`, one
  run) above it, and that track-c-gates judges T7 while this quest's criteria govern here. That
  matches the raw ratios (54.8/14.8, 77.5/17.1) and closes F2/F9's A1 half.
- A2: the doc names the 1.5× gate and states that its column is node 2's apply p99 for the other
  99 groups, not the leader's commit p99. I accept the argument: with three voters the leader
  commits through node 3, so its commit p99 cannot see node 2's slowed barrier, and apply at
  node 2 is where a head-of-line block would show. I also checked the leader commit p99 from the
  A2 log: slowed 20.3-21.6 ms against unslowed 22.1-27.1 ms at 2.8 ms, and 29.0-29.9 against
  29.6-31.3 ms at 12.2 ms, so the gate holds on that reading too. The doc's ranges (35.7-36.7
  against 37.3-52.0 ms; 49.8-50.3 against 49.8-51.1 ms) match the log, and the slowed arm is
  within 1.5× of the unslowed one at both RTTs.
- No blocking finding remains. F5 (no assertions in the bench or script) stays a non-blocking gap.
