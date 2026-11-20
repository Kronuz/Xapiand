# phase6-split review: design

Commit: Xapiand:6fa26d7b4df3fcb1ff93e95ca05c9fd508c4089a (`docs/design/phase6-split.md`, worktree `Xapiand-wt/explore/phase6-split`)
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The split is mostly sound: its citations check out against code and jobs, D5 (check before
suspending), D7 (nothing per open index) and D8 (scheduling stays with `light-share` and
`wide-search-loop`) hold, and one host cache for decoded and verified blocks (D6) is the right
one-mechanism call. Three things must change before quests are created from it: D1 does not weigh
the off-reactor mechanism Xapiand already has, the resident-set contract (D2, Q1) does not say how
a cache-served view meets prospect's contiguous decoders, and `strict-reads` cannot reach
prospect's reads with its file set and dependencies.

## Evidence I relied on

- `261113-165001-16b520` (x7-gate, Linux VM, d53d85c22): T6 at 2x RAM, `a1/log:36-49`. 330.5 major
  faults per 1,000 unloaded (1,909 of 1,983 on reactors, 96.3%), 110,093 loaded (550,674 of
  555,419, 99.1%), 0 in RAM. Worst loop p99 2,560 us unloaded both at 2x RAM and in RAM.
- `261111-072649-debd0a` (prospect phase-3 ratchets, `a1/log:52-`): allocations per query after a
  warm pass, every class and command.
- `261112-141805-74aaf9` (loop-scheduling E3, Linux VM), `base.json` unloaded phase: every HTTP
  loop's lag p50 640 us, p99 2,048 us.
- `261113-164926-89b9ef` (x7-gate T4/T5, macOS, 16 CPUs), `t45-2-cur.json`, `t45-3-cur.json`
  unloaded: all 16 HTTP loops p50 320 us, p99 512 us.

## What I checked myself

- Code at the split's pins: residency `79e9915` `residency.hh`; completion `f4c19b1`
  `linux_completion.h`, `bsd_completion.h`, `asio_completion.h`, `operation.h`, `io.h`; strata
  `491a1bdc` `reader.hh`, `section_io.hh`, `format.hh`, `docs/design/format-14-kv.md` D2,
  `docs/ARCHITECTURE.md` I/O; prospect `0aad777` `reader.hh`, `test/concepts_test.cc`,
  `tools/prospect_engine.cc`; Xapiand `docs/design/phase7c.md` D4, `docs/gates/phase7c.md`.
  Every line citation in the split that I opened matches.
- Consumers of residency today (`git grep residency` in strata, prospect, Xapiand `src`): strata
  uses only `verified_regions` (`reader.hh:151-171`, `section_io.hh:254`); prospect uses
  `residency::file` in one tool (`tools/prospect_engine.cc`, 10 sites) and `prefetch` only in a
  `static_assert` (`test/concepts_test.cc:23`); Xapiand nothing. Nobody calls `prefetch`.
- The VM: `strata/tools/vm/linux.sh:2,48` is `qemu-system-x86_64 -accel hvf` on this Mac; inside,
  `/sys/devices/system/cpu/cpuidle/current_driver` is `none` (idle vCPUs HLT), clocksource `tsc`.
- Probe `.scratch/review-phase6-split-design/timer_floor.{c,sh}` (Linux VM): a 1 ms absolute
  `timerfd` under `epoll`, which is what Asio's steady timer does. It measures wake lateness with
  and without `SCHED_IDLE` spinners on every vCPU (they keep vCPUs out of HLT), ABBA, 10 s per arm.
  Job `261117-231726-8348e4` lost its runner three times before starting, so I ran the same script
  once by hand under `quiet light review-phase6-timer`. Output is in `timer_floor.txt` beside the
  script. Lateness p50 / p99 / max in us:
  - Plain: 600 / 1,504 / 5,782 and 865 / 2,173 / 5,965.
  - Spinning: 779 / 3,129 / 14,403 and 1,128 / 10,555 / 158,381.

  With no Xapiand in the guest, the floor reproduces: p50 600-865 us against E3's 640 us. Keeping
  vCPUs out of HLT makes it worse, not better. This is a light run beside other holds, so read it
  as a direction, not a bound.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | D1 weighs one completion backend against two residency-owned ones, a strawman. It does not weigh the mechanism 7c made "the only work that leaves a reactor": the index chore on the host `WorkPool`, whose `query` class already serves blob reads. A Xapiand-side backend that runs `madvise(MADV_POPULATE_READ)` or `pread` as a `query` chore needs no completion change (drops `completion-read-depth` and Q3), no per-reactor queue, ring and fallback thread, and works the same on macOS (no 1 ms polled AIO) and where io_uring is blocked (container seccomp). Its cost is a pool worker blocked per range set, competing with flush and merge chores at 2x RAM loaded. Neither side has a number, so the choice is argued, not measured. | `docs/design/phase7c.md:25-26`, D4 `:193-205`; split D1 | Options: (a) keep D1 and state with a number why chores lose; (b) build the chore backend first (Xapiand only, small) and start `completion-read-depth` only if it misses A4 or starves flush and merge at 2x RAM loaded; (c) owner rewords A1 from "io_uring and POSIX AIO backends" to "an asynchronous residency backend". Recommend (b). The test is `xapiand-residency` A2's procedure (`stall_load.py --index-size 2xram`, Linux VM) run on both backends, with the merge backlog and flush latency reported beside the fault counts. |
| F2 | yes | gap | The resident-set contract (D2, Q1) leaves `view(range)` undefined where it matters. A cache-served view is one cache entry, a region of 4 KiB by default; prospect's postings cursor reads contiguous bytes across region boundaries from the section's base pointer and only re-verifies as it crosses them. A view over many cached regions is either a copy into contiguous scratch (a copy per read, and allocations against A7), a decoder change to read region by region (prospect `reader.hh`, held by `track-p` and by quest 6), or codec blocks aligned to regions (a format change, against D6's "no format change"). Freezing `acquire -> resident` before choosing one freezes the open question. A cheaper fix for the stale pass needs no cache and no new read API: `acquire` already learns from `mincore` which pages are absent, so it can reset those regions' verify state and verify them after it reads them in. That closes P2 with zero copies, leaving only eviction between `acquire` and the read (strict mode could `mlock` for the set's lifetime, or state the residual). | `format.hh:87` (`default_region_shift = 12`); prospect `reader.hh:1420-1430` (`need`), `:322-326`; residency `residency.hh:203-210`; split D2, D6 | `residency-api` A1 states view granularity (per region, or any contiguous range) and who crosses boundaries. D6 weighs "re-verify on residency transition" against the cache for strict mode, decided by P2 and A4's cost row. P2 is the test either way. |
| F3 | yes | defect | `strict-reads` (quest 8) lists strata files only and needs `residency-api` and `host-block-cache`. But prospect decodes postings, positions, terms and values straight from `section_handle::bytes` (the mapping) and only calls `verify`. A strict mode built in strata alone leaves every query read on the map, so A1 cannot hold for queries. | prospect `reader.hh:322-326`, `:476-485`, `values.hh:245-312`; strata `section_io.hh:251-256` | Add `prospect-residency-point` to its needs and prospect `reader.hh`/`values.hh` to its files, serialized after `track-p` and quest 6. Or scope strict mode to strata core reads (format 14's documents) and say so in A1. |
| F4 | no | gap | Quest 4's reason and size understate A7's gap. After warm-up, every class allocates 38 to 123 times per query at the median (union up to 411, 21-178 KB), not only the term class's growth to 317. "Zero on four classes, 2 sessions, 600 lines" is unsized until the sites are attributed. Some may be in the harness, outside the quest's files. | `261111-072649-debd0a` `a1/log:52-` | Make A1 of quest 4 a site attribution (a heap profile of steady-state queries), then size. |
| F5 | no | gap | File sets overlap, so "in parallel now" does not hold. Quest 4 and quest 6 share `searcher.hh`, `slice.hh` and `scorer.hh`. Active `track-p` holds `reader.hh`, `search.hh` and `docs/perf/**`, which quests 4 and 6 (and F3's fix) need. | `campaign get track-p`; split quests 4 and 6 | Order `track-p`, then 4, then 6. Quest 6's resident-set path then runs under quest 4's allocation hook as an extra A1 case. |
| F6 | no | gap | D3 makes residency's core take completion's `Mapping`/`File` while `faultlab-seam` (active) changes `io.h`: it adds `File::advise` and `File::map_shared`. It also makes a header-only library that needs only task and crc32c depend on Detent's durability library for one abstract class. | `faultlab-seam` files and A1-A2; residency `CMakeLists.txt:7-17` | Put `residency-api` after `faultlab-seam`, and use F8's `File::advise` for the sync backend if its advice covers WILLNEED (one advice path). Keep the core's source a byte span plus an optional `File`, with the completion backend in its own target, as task keeps its Asio adapter. |
| F7 | no | gap | D7 holds per open index, which is what millions of indexes need. Per reactor it is understated. Each completion queue starts a fallback `std::thread` (`linux_completion.h:72`, `bsd_completion.h:60`), an eventfd or kqueue, and a ring sized 2 today (`:209-210`). `IOSQE_ASYNC` (`:127`) sends every read to an io-wq kernel worker. Without io_uring, the one fallback thread serializes the reads, so depth buys nothing there. T1's census (16b520's `GATE7C_ALLOW`) will see one unnamed thread per reactor. | code cited | `xapiand-residency` A1 names the thread, adds it to the census, and states io-wq workers at depth x reactors. `completion-read-depth` states fallback depth. |
| F8 | no | gap | D5's in-RAM cost multiplies on wide searches: one `mincore` per range set, two or three points per segment, every segment of every index. Quest 6 A3 measures single-index classes only; the many-indexes workload is per-index single-thread cost. | split D5, quest 6 A3; SR3 row `docs/gates/phase7c.md:88` | Add a wide-search row (2,500 indexes, the SR3 harness) to quest 6 A3. Option: skip the check when the node is under no memory pressure (a host flag), so in-RAM nodes pay zero syscalls. |
| F9 | no | gap | Coverage lists A7 and A8 as covered, but no quest moves them. A8 disk is at 0.993x whole, postings 1.012x and terms 1.123x against the 0.8x/0.9x target. A7 serving memory is at 0.94x of Tantivy against 0.7x: the cache charges bytes, it does not reduce them. A6 already passes (0.424x Tantivy-st's time). | prospect `docs/gates/phase-3-ratchets.md:113`, `:120-122` | Coverage marks A7-serving and A8 as unowned expected misses for the owner, or names the quests. |
| F10 | no | nit | The unloaded 2x-RAM faults do not show as a stall at p99: worst loop 2,560 us and light p99 6.6 ms, against 2,560 us and 6.3 ms in RAM. The stall evidence is the loaded run, which also carries `memory.high` reclaim throttling. "99.1%" is the loaded share; unloaded it is 96.3%. A4's count reason is measured; the latency gain is not. | `261113-165001-16b520` `a1/log:37-49` | Say so in the reasons table; `xapiand-residency` A3 reports the latency. |
| F11 | no | nit | Q2 (c)'s 256 us bar is stricter than the Mac it would replace (unloaded HTTP p99 512 us, 89b9ef). "Idle floor" is the unloaded phase's 2,048 us histogram bucket at the probe's 1 ms busy interval, not an idle node. | `261112-141805-74aaf9`, `261113-164926-89b9ef` | Bar: at most the Mac's 512 us. Cite E3's job for the floor, not the design note. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Answers to the lead's questions

1. **Measured reasons.** Linux async residency rests on T6's count (measured, F10 on what it does
   not show). FreeBSD and macOS backends, the verified cache for indexes much larger than RAM (M9),
   and D4's `MADV_POPULATE_READ` are unproven, and the split says so, with P1, P2 and A4 as their
   first jobs. Two reasons are unproven and not stated as such: the choice of completion over a
   chore (F1), and the cache over re-verifying on residency transition for strict mode (F2).
   Quest 4's reason is understated (F4).
2. **One backend on completion's drivers.** Right against two residency-owned backends: a second
   ring per reactor would duplicate completion. Not yet right against the chore path, the
   mechanism 7c already uses to leave a reactor (F1). Either way it holds for millions of indexes:
   nothing is per open index beyond `verified_regions`' five bytes per region. The per-reactor
   costs are a thread, a ring and io-wq workers (F7), and per-query `mincore` on wide searches (F8).
3. **Q1.** Changing the interface is free in callers: nobody calls `prefetch` (one `static_assert`),
   and `residency::file` lives in one prospect tool. So the frozen-interface cost is not the issue.
   D2's reason is. Answer: drop `residency::file` now (D3, after `faultlab-seam`), but do not freeze
   `acquire -> resident` until F2's view contract is chosen. If strict mode takes the
   re-verify-on-transition path, `prefetch` can stay (returning which regions it brought in), and
   there is no second read API to avoid.
4. **Order and file sets.** `strict-reads` misses prospect and quest 6 (F3). Quests 4, 6 and
   `track-p` overlap (F5). `residency-api` should follow `faultlab-seam` (F6). Quest 2 does not
   touch `io.h` or `posix.h`, so no file overlap with `faultlab-seam`, but F1 may remove it. Quest 4
   is unsized (F4).
5. **Q2.** See below.

### Q2: gate-row placement and the VM floor

The floor is real and reproduced. E3 (`261112-141805-74aaf9`): every HTTP loop p50 640 us, p99
2,048 us unloaded. T6 in RAM (`16b520`): worst loop 2,560 us unloaded. The Mac at the same
commit family (`89b9ef`): 320 us and 512 us. The VM is a QEMU guest on this Mac under HVF with no
cpuidle driver, so an idle vCPU halts, and each timer wake goes through QEMU's emulated APIC timer
on the host. My probe shows a bare `timerfd` in the guest wakes 600-865 us late at p50 and
1.5-2.2 ms at p99, with nothing else running there, so the floor is not Xapiand's. Spinning every
vCPU, the runtime analog of `idle=poll`, does not lower it: p99 goes to 3.1-10.6 ms, because
eight vCPUs burning host cores beside other holds only add contention. So the cause is the host's
side of HVF timer delivery (QEMU emulates the APIC timer in user space on x86 HVF), not guest
idle. Guest tuning, which is what option (c) proposes, is unlikely to move the floor. Two host-side
knobs are untried. One is the QEMU process's QoS or timer-coalescing class on macOS
(`taskpolicy`, or running it at a latency-critical QoS). The other is a bare-metal Linux host,
option (b). Neither one fixes the floor for certain. Re-run the probe under `quiet exclusive` with
QEMU at a raised QoS before spending more on (c).

Placement (a) is sound. The Mac's floor (512 us) is under the 1,024 us edge, `quiet exclusive`
holds it alone, and the control loop's 100 ms probe is already read only while busy (D53). Its
known caveats are the Python load generator on the same host and macOS's polled AIO, which does
not matter while D5 skips the backend in RAM. A4 belongs in the Linux VM: a fault count is not a
timing. Narrow (c) to the host-QoS probe above, with the bar at the Mac's 512 us, not 256 us (F11).

## Not run

- No build or test of any library. No P1 or P2. No io_uring `MADV_POPULATE_READ` check.
- Nothing on FreeBSD. No check of Detent's in-flight completion work beyond the branches merged
  into completion `main`. `namespace-cleanup` (planned) names a `CompletionScheduler` removal I did
  not read.
- The chore backend's cost (F1) and re-verify on transition (F2) are designs I propose, not
  measured.

## Re-check

Commit: Xapiand:fcc0c376d
Verdict: APPROVE
Date: 2026-11-18

Scope: whether F1 to F3 are closed, and whether revision 2 (`git log 6fa26d7b4..fcc0c376d`, one
commit, `docs/design/phase6-split.md` only) broke anything it touched. No new survey and no new
probe.

- **F1: closed.** D1 builds the chore backend first (`pool_backend` on the `query` class). Quests
  7 and 8, the completion work, start only if `xapiand-residency` A2 misses A4 or pushes flush p99
  or the merge backlog outside the A/A band. The same procedure then runs on both backends and the
  loser is deleted. D7 and A1 keep T1's census unchanged.
- **F2: closed.** D2 keeps `prefetch` and partitions the ranges in place, returning how many it
  brought in. Views are always the mapping's contiguous bytes, so the view-across-cache question is
  gone. D6 adds `invalidate` with a generation that `verify` compare-exchanges against, and quest
  1's A2 models every interleaving of `verify` and `invalidate`. The residual (an eviction and
  re-read outside `prefetch`) is stated, and `strict-reads` A4 owns it.
- **F3: closed.** `strict-reads` covers strata's core reads and needs `prospect-residency-point`.
  The query side, including invalidation, per-region values verify and the A4 damage test, moved
  into quest 3, which runs after `track-p` and `query-zero-alloc`.

Non-blocking notes on what the revision touched:

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | no | defect | Quest 3 says that under strict mode "prospect's pinned structures are copies", but its files omit `reader.hh`. Today the term index is already copied (`reader.hh:308`, `:477`), while sections checked whole at open are then read from the map (`reader.hh:428`), and so are the norms (`norms_at_`). | prospect `reader.hh:308-318`, `:428`, `:477` | Add `include/prospect/reader.hh` to quest 3's files. `track-p` releases it before quest 3 starts, so nothing new conflicts. |
| R2 | no | gap | D7 puts D6's generation "inside the same byte" as the 2-bit state, which leaves about 6 bits. A `verify` whose CRC (up to 1 MiB) spans 64 invalidations of its region wraps the generation, and the compare-exchange then succeeds with a stale "good". | `residency.hh:186`, `:234` (one `uint8_t` per region) | Quest 1's A1 states the generation width and why a wrap cannot happen (or widens the state), and A2's model includes the wrap. |
| R3 | no | nit | D3 deletes `residency::file`, but `pinned::copy(const file&, ...)` takes one, and quest 5's A2 uses `residency::pinned`. | `residency.hh:139` | `residency-api` A1 moves `pinned::copy` to the mapped span. |
