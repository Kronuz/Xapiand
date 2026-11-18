> Status: DRAFT (2026-11-17), ready for review (lens `design`). The lead owns the final quest state.

# Phase 6 split: async residency, the host block cache, and the gate

Reads Xapiand master `581febb1d`; strata `491a1bdc`; prospect `0aad777`; residency `79e9915`; completion
`f4c19b1`. Splits the umbrella quest `phase6` (`.campaign/xapiand/quests/phase6.md`, A1 to A10) into
quests in `residency`, `completion`, strata, prospect and Xapiand. No code; no measurement was run for
this draft, and every number below cites the job or document that produced it.

## Summary

1. Reactor faults at twice RAM are the one measured stall residency fixes: 330.5 major faults per
   1,000 queries unloaded and 110,093 loaded, 99.1% of them on the reactors (x7-gate T6, job
   `261113-165001-16b520`, `docs/gates/phase7c.md:74`). Nothing in strata or prospect asks for
   residency today: `grep -rn "prefetch\|madvise" strata/include prospect/include` is empty, and
   `sync_backend` only advises (`residency.hh:249-267`).
2. io_uring and POSIX AIO are already completion's drivers (`linux_completion.h:261-272`,
   `bsd_completion.h:111-133`). Phase 6 builds **one** asynchronous residency backend over completion,
   not two (D1). What completion lacks is depth: each queue holds one operation
   (`linux_completion.h:102`, `bsd_completion.h:93`).
3. The verified block cache is format 14's host CLOCK cache (strata `docs/design/format-14-kv.md`,
   D2), holding verified raw blocks beside decoded ones (D6). Its first reason is correctness, shown
   by a test, not a timing: a verified region stays "good" forever (`residency.hh:203-210`), so a
   page re-read from a damaged medium after eviction is served unchecked (strata
   `docs/design/media-faults.md:109-111`).
4. Scheduling tuning needs no new quest: `light-share` (planned) owns T4 and T5 and `wide-search-loop`
   owns SR3. Phase 6 re-runs A2 and A3 after them (D8).
5. Nine quests, ordered by what each unblocks. Two need the owner: Q1 changes residency's frozen
   Phase 1 interface, and Q2 decides where the bare-metal rows run.

## What this depends on and changes

| Document | Relation |
|---|---|
| strata `docs/PLAN.md:640-649` (Phase 6), `:121-253` (Ratchets) | Implements; A5 to A8 are its Phase 6 rows |
| strata `docs/ARCHITECTURE.md:173-183` (suspension, guarantee), `:204-216` (I/O) | Implements. Changes `:207-208` and `:211-214`: one backend on completion's drivers (D1), and the verified cache is format 14's host cache (D6) |
| strata `docs/design/format-14-kv.md` D2 (`:215-263`) | Extends: the cache gains a verified raw-block kind. Its lock-free design review is shared (`host-block-cache`) |
| Xapiand `docs/design/phase7c.md` D1, D6 item 3 (`:271`), T4 to T6 (`:457-459`) | Depends: the reactor model; D6 item 3 is what `xapiand-residency` closes |
| `.campaign/xapiand/PLAN.md` D15 (`:197`), D39 (`:221`) | D15: A4 gates here. D39: Phase 6 follows the release, so its defaults flip after it (D9) |
| residency `residency.hh` (Phase 1's frozen interface, strata `PLAN.md:313`) | Changes the `backend` concept and drops `residency::file` (D2, D3; Q1) |
| completion `include/completion/*_completion.h` | Changes the queues to hold N reads. completion is Detent's (strata `ARCHITECTURE.md` "Libraries", rule 3): the lead coordinates `completion-read-depth` with Detent's lead |
| Quests `light-share`, `wide-search-loop`, `track-p` | Depends: they own A2, A3 (scheduling) and A5 (query speed) work; Phase 6 measures |

## Measured reasons, per mechanism

| Mechanism | Reason | Evidence | First targeted job if unmeasured |
|---|---|---|---|
| Async residency on Linux (io_uring) | Evaluation faults on the reactor at 2x RAM: 330.5 per 1,000 unloaded, 110,093 loaded; loaded light p99 23.2 s, worst loop p99 786 ms. In RAM: 0 per 1,000 | `261113-165001-16b520`, `docs/gates/phase7c.md:74` | Measured |
| Async residency on FreeBSD (POSIX AIO) | Same mechanism, unmeasured on FreeBSD | None | P1 below, in the FreeBSD VM |
| Async residency on macOS (POSIX AIO) | No gated row: A4 runs on Linux. macOS allows 16 outstanding AIO requests per process on 4 kernel threads (`sysctl kern.aioprocmax kern.aiothreads`, this host, 2026-11-17), and its completions are polled, not notified (`bsd_completion.h:80-86`, `:118-123`). With the index in RAM, D5's check skips the backend, so macOS gains nothing measurable until an index exceeds RAM there | sysctl read | P1 on the Mac (light) states the fault count; no gate depends on it |
| Verified block cache, strict mode | A region verified once is trusted after its page is evicted and re-read; pinned sections are checked once at open and then read from the map (strata `reader.hh:159-161`) | Code: `residency.hh:203-210` | P2 below, in the Linux VM |
| Verified block cache, index much larger than RAM | ARCHITECTURE claims it (`:211-213`); unmeasured: RocksDB survey M9 (`rocksdb-survey.md:178`, `:250`) | None | `host-block-cache` A4: readrandom at 2x and 10x the cgroup cap, mmap against `pread` into the cache, Linux VM |
| Scheduling tuning | T4 worst loop p99 6,144 us against 1,024 us; T5 loaded light p99 33.0 ms against 8.91 ms (`261113-164926-89b9ef`, `docs/gates/phase7c.md:72-73`) | Measured | No new mechanism here: `light-share` |
| Zero allocations per query (A7) | The term class grows to 317 allocations per query at `TOP_1000` | `261111-072649-debd0a`, prospect `docs/gates/phase-3-ratchets.md:121` | Measured |

**P1, the fault probe** (new, residency `bench/fault_probe.cc`, built in `residency-api`): map a file,
evict it (`posix_fadvise(POSIX_FADV_DONTNEED)`, plus `msync(MS_INVALIDATE)` on FreeBSD), confirm with
`mincore` that no sampled page is resident (the probe exits 2 otherwise, so the eviction is checked,
not assumed), then read 1,000 random 16 KiB ranges on one thread through a backend. It prints major
faults per 1,000 ranges from `getrusage(RUSAGE_THREAD)` and the thread's held time per range (p50,
p99, max). `campaign job submit --quest residency-api --for A3 --level light --minutes 10 -- ...` in
each VM: fault counts are not timings, so a light VM job decides them.

**P2, the stale-verify test** (new, residency `test/stale_verify.cc`, Linux only): write a file, map
it, `verify()` every region (all good), rewrite one region's bytes with `O_DIRECT` so the page cache
is bypassed, evict with `POSIX_FADV_DONTNEED` (checked by `mincore`), and read through the map: the
bytes no longer match their CRC-32C, yet `verify()` returns true. Against `sync_backend` it
reproduces the hole; against `strict-reads`' backend the same test passes, so it is that quest's
`--fails-without` evidence.

## Decisions

**D1. One asynchronous backend, on completion's drivers.** residency gains `completion_backend`: it
issues completion `Read` primitives for the ranges that are not resident, and the host's driver
decides io_uring (Linux) or POSIX AIO (FreeBSD, macOS). *Lost:* separate io_uring and AIO backends in
residency, which would duplicate completion's two drivers and give each reactor a second ring. A1's
"io_uring and POSIX AIO residency backends" is met by this backend tested natively on each driver.

**D2. A backend returns a resident set.** `co_await b.acquire(source, ranges)` returns a `resident`
whose `view(range)` is valid while it lives. An mmap backend's views point into the mapping; the
cache backend's into pinned cache entries. *Lost:* keeping `prefetch -> task<void>`, under which the
cache backend cannot hand out its bytes, so strict mode would need a second read API beside the
mapping, two mechanisms for one read.

**D3. One mapping type.** residency takes the engine's mapping (completion `Mapping` and `File`,
`io.h:62-94`) and `residency::file` is deleted; its callers move (prospect `tools/prospect_engine.cc:220-221`,
`test/concepts_test.cc`). strata maps through completion already (`index.hh:2531`, `:2937-2940`) and
never uses `residency::file`. *Lost:* two mapping types, one unused by the engine.

**D4. How a range becomes resident.** Default: read it into a per-reactor scratch buffer through
completion, which warms the page cache and leaves only minor faults. Candidate on Linux: `IORING_OP_MADVISE`
with `MADV_POPULATE_READ`, which copies nothing and populates the page tables; whether io_uring
passes that advice is unverified. `residency-async` A2 decides with P1 run against both. A range
evicted between `acquire` and the read still faults; A4's 1 per 1,000 allows for it, and P1 counts it.

**D5. Check before suspending.** `acquire` asks `mincore` for the ranges first and suspends only when
some page is missing, so an index in RAM (every 7c row but T6) pays one syscall per range set and no
hop. `prospect-residency-point` A3 measures that cost.

**D6. One host cache for decoded and verified blocks.** The verified block cache is format 14's
CLOCK cache with a second key kind (segment, region index): a miss `pread`s the region at the
format's region granularity (strata `reader.hh:149-157`; no format change) and verifies it on load;
one permit on the node's `byte_budget`. *Lost:* a separate verified cache (two caches competing for
one budget, two lock-free reviews); strict mode as `mlock` plus re-verify per query (a syscall pair
per range, bounded by `RLIMIT_MEMLOCK`, and no help for indexes much larger than RAM).

**D7. Per reactor, never per index.** One completion queue and one scratch buffer per reactor, sized
by the queue depth; an open index pays nothing for residency beyond the `verified_regions` it holds
today (about five bytes per region, `residency.hh:164-170`). That is the scale statement each quest
writes (A9).

**D8. Scheduling tuning is not a Phase 6 quest.** `light-share` (D86's P1, D90) and `wide-search-loop`
own the in-loop work; `loop-scheduling` showed the remainder is runnable loops off their CPUs
(`docs/design/loop-scheduling.md:32-37`). The 200 us slice (phase7c D2) is tuned there.

**D9. Landing.** Each backend lands behind an option, off by default, with its tests (`phase6`, "May
land early"). Defaults flip in `xapiand-residency` after A4 passes, after the release (D39).

**D10. Gate machines.** Linux VM (strata's, 8 vCPUs, 16 GB; BENCHMARKS rule 4): io_uring tests, A4
(cgroup `memory.high`, per-thread faults from `/proc`, as T6 ran), P2, the ratchet rows A5 to A8.
FreeBSD VM (strata's): AIO tests, P1. Neither VM can judge A2: its idle floor is 2.05 ms against the
1,024 us edge (`docs/design/loop-scheduling.md:328`, `:345`). This draft did not touch either VM.

## Quests

In the order they unblock. Sizes count contributor sessions and lines changed.

### 1. `residency-api`: a resident set over the engine's mapping

- Repo `residency`; files `residency.hh`, `test/**`, `bench/**`, `docs/**`, `README.md`, `CMakeLists.txt`.
- Risk high (a frozen public API, Q1); lens `design`. Needs none; after none. Unblocks 5, 6, 8.
- A1 [artifact] The contract: `acquire`, `resident`, the source type, D5's check, as `docs/resident-set.md`.
- A2 `sync_backend` on the new concept, `residency::file` gone, tests green on Linux, macOS and FreeBSD with ASan, UBSan and TSan: CI plus `campaign verify residency-api --for A2`.
- A3 P1's baseline: `sync_backend`'s faults per 1,000 ranges on evicted data in the Linux and FreeBSD VMs (light jobs).
- A4 P2 reproduces the stale pass in the Linux VM (light job; the evidence `strict-reads` needs).
- Serves A1, A4 (baseline), A9. Size: 1-2 sessions, about 400 lines.

### 2. `completion-read-depth`: N reads in flight per queue

- Repo `completion`; files `include/completion/{linux,bsd,native,asio}_completion.h`, `completion.h`, `test/**`, `docs/**`.
- Risk high (Detent's durability library); lens `correctness`. Needs none; after the lead's
  coordination with Detent. Unblocks 5.
- A1 A queue accepts up to its depth of `Read` primitives and refuses the next without side effects; writes and syncs keep one-at-a-time order: tests with `HeldCompletion` reordering N completions (`campaign verify`).
- A2 Native depth on io_uring in the Linux VM and on AIO in the FreeBSD VM (stats' `native_completed` equals submitted): light jobs.
- A3 macOS: `EAGAIN` past `kern.aioprocmax` falls back to queueing, never an error, and polling stays bounded; idle wake-ups unchanged against `main` (Xapiand `docs/perf/idle-wakeups.md` method): strict job on the Mac.
- A4 Detent's suite passes at the change (a job at Detent's pin bump).
- Serves A1. Size: 2 sessions, about 500 lines.

### 3. `host-block-cache`: format 14's CLOCK cache, with verified blocks

- Repo strata; files `include/strata/core/block_cache.hh` (new), `include/strata/core/budget.hh`, `test/**`, `docs/design/format-14-kv.md` (D2 only), `docs/mechanisms/block-cache.md`.
- Risk high (lock-free); lens `correctness`, with format 14's concurrency design review first. Needs none. Unblocks 8 and format 14's step 3.
- A1 [artifact] The design review of D2 plus the verified kind, approved.
- A2 An executable model of hit, insert, sweep and release, enumerating interleavings, plus a TSan stress: jobs.
- A3 The budget asks the cache to evict before a buffer flushes early: a test with a full cache and a buffer draw.
- A4 M9: readrandom at 2x and 10x the cgroup cap, mmap against `pread` into the cache, in the Linux VM; ratio and interval stated, a loss stated as one (exclusive job, 10 minutes, ABBA).
- Serves A1, A7 (cache bytes charged), A9. Size: 3 sessions, about 900 lines.

### 4. `query-zero-alloc`: no heap allocation per query in steady state

- Repo prospect; files `include/prospect/{searcher,scorer,operators,search,slice}.hh`, `test/**`, `docs/perf/**`.
- Risk medium; lens `correctness`. Needs none. Unblocks 9 (A7's second half).
- A1 `allocs.py` reports 0 allocations per query after warm-up on the term, conjunction, disjunction and phrase classes at every command (the class that reached 317 included): light job with `gate3.sh allocs`.
- A2 A test that fails on any allocation in a steady-state query on those paths (an allocation-counting hook): `--fails-without` on a mutant that allocates.
- A3 Query latency within the A/A band of `main` on the four classes: strict job, ABBA.
- Serves A7. Size: 2 sessions, about 600 lines.

### 5. `residency-async`: one backend on completion's drivers

- Repo `residency`; files `residency_completion.hh` (new), `test/**`, `bench/**`, `docs/**`.
- Risk high; lens `correctness`. Needs `residency-api`, `completion-read-depth`. Unblocks 7.
- A1 Tests with `HeldCompletion`: a set resolves only after every read completes; a failed read surfaces as an error from `acquire`, never a resident claim; cancellation keeps buffers alive (completion's rule): `campaign verify`, with ASan and TSan.
- A2 D4 decided: P1 at depth 8 and 32 with read-to-scratch and (Linux) `MADV_POPULATE_READ`; the chosen one has at most 1 major fault per 1,000 ranges on the calling thread: Linux VM light jobs.
- A3 Native on io_uring (Linux VM) and AIO (FreeBSD VM): P1 at most 1 per 1,000 on both; the Mac's count reported.
- A4 In RAM, `acquire` returns without suspending and with no read submitted (D5): a test counting submissions.
- Serves A1, A4. Size: 2 sessions, about 500 lines.

### 6. `prospect-residency-point`: queries ask for their ranges before they read

- Repo prospect; files `include/prospect/{searcher,slice,reader,scorer}.hh`, `include/prospect/strata.hh`, `test/**`, `tools/prospect_engine.cc`, `CMakeLists.txt`.
- Risk high (the query path the release ships; lands behind an option, D9); lens `correctness`. Needs `residency-api`; after none. Unblocks 7 and 9.
- Contract: `prospect::slice` (already the per-request context, Xapiand `src/engine/strata/search.cc:734`) carries an optional type-erased backend; without one, behavior is today's. Two points per segment: the term-dictionary blocks the query's terms land in, then their postings (positions for phrase and near); stored fetches are a third.
- A1 With a held backend, a query reads no byte outside the sets it acquired: a test whose source records every touched page against the acquired ranges, over every query class.
- A2 Oracle parity and `verify` unchanged with the backend on: jobs.
- A3 In RAM, the four classes within the A/A band of `main` with the backend on (D5's cost): strict job, ABBA.
- Serves A4, A5 (no regression). Size: 3 sessions, about 900 lines.

### 7. `xapiand-residency`: the reactors stop faulting at twice RAM

- Repo Xapiand; files `src/engine/strata/search.cc`, `src/engine/host*.h`, `src/opts.cc`, `harness/stall_load.py`, `docs/mechanisms/residency.md`.
- Risk medium; lens `system`. Needs `residency-async`, `prospect-residency-point`. Unblocks the A4 row.
- A1 One backend per reactor on its completion queue, registered on the loop (`asio_completion.h`); none per index: a census test.
- A2 **A4**: major faults on reactor threads at most 1 per 1,000 queries at 2x RAM, unloaded and loaded (`stall_load.py --index-size 2xram`), against T6's 330.5 and 110,093: Linux VM jobs, before and after.
- A3 T4 and T5 at 2x RAM reported beside the in-RAM run (not gated in the VM, D10).
- A4 The default flips on after A2 passes and after the release (D9): E2E green.
- Serves A4, A2, A3. Size: 1-2 sessions, about 300 lines.

### 8. `strict-reads`: every read verified, through the host cache

- Repo strata; files `include/strata/core/{reader,section_io,index}.hh`, `test/**`, `docs/mechanisms/strict-reads.md`.
- Risk high; lens `durability`. Needs `residency-api`, `host-block-cache`. Unblocks nothing on the gate path.
- A1 P2 passes with strict mode on and fails without it (`--fails-without`): Linux VM job.
- A2 Pinned sections are copies under strict mode (`residency::pinned`), so no checked byte is re-read from the map: test.
- A3 A damaged region is a `corrupt_segment` error and the segment heals (media-faults' path), not a wrong result: faultfs test.
- A4 Strict mode's cost on all four axes in the Linux VM, stated with both sides' numbers.
- Serves A1, A9. Size: 2-3 sessions, about 700 lines.

### 9. `phase6-ratchets`: the Phase 6 rows on the gate VM

- Repo prospect; files `bench/**`, `docs/gates/phase-6-ratchets.md`.
- Risk medium; lens `system`. Needs none; after `query-zero-alloc`, `prospect-residency-point`, `track-p`.
- A1 Calibration with PISA binding (QG-R28, F13-R45): every engine on the sample, fastest per row named: exclusive jobs.
- A2 **A5** queries at most 0.7x on every class; **A6** indexing at least 1.3x; **A7** memory at most 0.7x; **A8** disk at most 0.8x whole and 0.9x per section: gate VM jobs by PLAN's passing rule, each missed row carried by the owner or blocking.
- A3 [artifact] The report, every row with ratio, interval, commit and platform: `docs/gates/phase-6-ratchets.md`.
- Serves A5 to A8. Size: 2 sessions plus machine time (about 15 exclusive jobs of 10 minutes).

### The umbrella keeps

`phase6` runs A2 and A3 at its commit after `light-share`, `wide-search-loop` and `xapiand-residency`
land, gathers A9 from the quests (sanitizers per quest; no decoder is added, so no new fuzz target
unless `strict-reads` changes one), and writes A10 (`docs/gates/phase6.md`).

## Coverage

| Row | Quests | Machine |
|---|---|---|
| A1 backends and cache, Linux and FreeBSD | 1, 2, 3, 5, 8 | Linux VM, FreeBSD VM, CI |
| A2 reactor p99 stall | `light-share`, `wide-search-loop`; re-run by `phase6` | bare metal (Q2) |
| A3 light-query p99 | `light-share`; re-run by `phase6`; 7 reports it at 2x RAM | bare metal (Q2) |
| A4 faults at 2x RAM | 1 (baseline), 5, 6, 7 | Linux VM |
| A5 queries | `track-p`, 9 | Linux VM |
| A6 indexing | 9 | Linux VM |
| A7 memory, zero allocations | 3, 4, 9 | Linux VM |
| A8 disk | 9 (format work outside Phase 6) | Linux VM |
| A9 every gate | each quest, gathered by `phase6` | CI, VMs |
| A10 report | `phase6` | none |

## Open questions

**Q1 (owner). Change residency's frozen interface?** Phase 1 froze "the residency interface with a
synchronous backend" (strata `PLAN.md:313`). D2 and D3 change it: `prefetch -> task<void>` becomes
`acquire -> resident`, and `residency::file` goes. Options: (a) change it, migrating the two prospect
callers (the libraries are ours, nothing forwards); (b) keep `prefetch` and add the cache as a
second read API. *Recommendation:* (a); (b) leaves two read paths for one read.

**Q2 (owner). Where do the bare-metal rows run?** A2 and A3 cannot be judged in the Linux VM (2.05 ms
idle floor), the M1 is retired, and AWS is ruled out. Options: (a) the development Mac under `quiet
exclusive` for A2 and A3 (macOS, AIO polled), A4 as a count in the Linux VM; (b) wait for a
bare-metal Linux host; (c) try to lower the VM's floor first (a guest tuned for latency) and gate
there if it drops under 256 us. *Recommendation:* (a), with (c) as a 10-minute probe that could
move A2 to Linux.

**Q3 (lead). completion's depth with Detent.** `completion-read-depth` changes the queue contract
Detent consumes; the lead orders it against Detent's in-flight completion quests before it starts.

## First buildable steps

1. `residency-api` A1 and P1/P2 (A3, A4): the contract and the two probes, which give every later
   quest its baseline.
2. In parallel: `completion-read-depth` (after Q3), `host-block-cache` A1 (the review), and
   `query-zero-alloc`, which need nothing.
