> Status: DRAFT (2026-11-17), revision 2, ready for re-review (lens `design`). Folds F1 to F11 of
> `.campaign/xapiand/reviews/phase6-split-design.md`. The lead owns the final quest state.

# Phase 6 split: residency off the reactor, verified reads, and the gate

Reads Xapiand master `581febb1d`; strata `491a1bdc`; prospect `0aad777`; residency `79e9915`; completion
`f4c19b1`. Splits the umbrella quest `phase6` (`.campaign/xapiand/quests/phase6.md`, A1 to A10) into
quests in `residency`, strata, prospect and Xapiand. No code; no measurement was run for this draft,
and every number cites the job or document that produced it.

## Summary

1. Reactor faults at twice RAM are the one measured problem residency fixes: 330.5 major faults per
   1,000 queries unloaded (96.3% on reactors) and 110,093 loaded (99.1%), x7-gate T6
   (`261113-165001-16b520`, `docs/gates/phase7c.md:74`). Nothing asks for residency today: nobody
   calls `prefetch`, and `sync_backend` only advises (`residency.hh:249-267`).
2. The first backend runs as a `query` chore on the host `WorkPool`, the mechanism 7c made the only
   way off a reactor (phase7c D4, `:193-205`): no completion change, no per-reactor ring or thread, the
   same on Linux, FreeBSD and macOS (D1). completion's io_uring and POSIX AIO drivers are a
   contingent pair of quests, started only if the chore backend misses A4 or slows flush and merge,
   decided by one procedure run on both.
3. Strict mode verifies reads without a second read path: `prefetch` reports the regions it brought
   in, their verify state resets, and the readers' existing per-region `verify` checks them again (D2,
   D6). The format 14 CLOCK cache stays format 14's; whether it also serves indexes much larger than
   RAM is RocksDB survey M9's question, outside the gate path.
4. Scheduling tuning needs no new quest: `light-share` owns T4 and T5, `wide-search-loop` SR3 (D8).
5. Six quests on the gate path, two contingent. Three questions for the owner: A1's wording (Q1),
   the bare-metal rows (Q2), and A7's serving memory and A8, which no quest here moves (Q3).

## What this depends on and changes

| Document | Relation |
|---|---|
| strata `docs/PLAN.md:640-649` (Phase 6), `:121-253` (Ratchets) | Implements; A5 to A8 are its Phase 6 rows; Q1 rewords its item line |
| strata `docs/ARCHITECTURE.md:173-183`, `:204-216` (I/O) | Implements. Changes `:207-208` (residency runs as a pool chore, D1) and `:211-214` (strict mode re-verifies on residency transition, D6) |
| strata `docs/design/format-14-kv.md` D2 | Unchanged: the CLOCK cache stays a decoded-block cache; M9 decides any wider role |
| Xapiand `docs/design/phase7c.md` D1, D4 (`:193-205`), D6 item 3 (`:271`), T1, T4 to T6 | Depends: the reactor model and chore classes; D6 item 3 is what `xapiand-residency` closes |
| `.campaign/xapiand/PLAN.md` D15 (`:197`), D39 (`:221`) | D15: A4 gates here. D39: defaults flip after the release (D9) |
| residency `residency.hh`, Phase 1's frozen interface (strata `PLAN.md:313`) | Changes: the source becomes the mapped bytes, `prefetch` reports what it brought in, `verified_regions` gains `invalidate`, `residency::file` goes (D2, D3) |
| Quest `faultlab-seam` (completion `io.h`: `File::advise`, `map_shared`) | `residency-api` runs after it (D3) |
| Quests `track-p`, `light-share`, `wide-search-loop` | `track-p` holds prospect `reader.hh`, `search.hh`, `docs/perf/**`: quests 2 and 3 follow it (F5). The other two own A2 and A3 |

## Measured reasons, per mechanism

| Mechanism | Reason | Evidence | First targeted job if unmeasured |
|---|---|---|---|
| Residency off the reactor, Linux | Count: 330.5 per 1,000 unloaded, 110,093 loaded, 0 in RAM. Latency: the loaded run (light p99 23.2 s, worst loop p99 786 ms) also carries `memory.high` reclaim throttling; unloaded at 2x RAM the stall does not show at p99 (worst loop 2,560 us, light p99 6.6 ms, against 2,560 us and 6.3 ms in RAM). The count is the measured reason; the latency gain is not | `261113-165001-16b520` `a1/log:36-49` | `xapiand-residency` A3 reports the latency |
| Same, FreeBSD | Unmeasured | None | P1, FreeBSD VM |
| Same, macOS | No gated row; in RAM, D5 skips the backend | None | P1 on the Mac, reported |
| completion's async drivers (contingent) | Unproven: whether the chore backend starves flush or merge, or misses A4 | None | `xapiand-residency` A2 on the chore backend decides it |
| Strict verified reads | A verified region stays "good" after its page is evicted and re-read (`residency.hh:203-210`); pinned sections are checked once at open and then read from the map (strata `reader.hh:159-161`) | Code | P2, Linux VM |
| CLOCK cache for indexes much larger than RAM | Claimed (`ARCHITECTURE.md:211-213`), unmeasured (M9, `rocksdb-survey.md:178`, `:250`) | None | Format 14's step 3, outside the gate path |
| Scheduling | T4 worst loop p99 6,144 us against 1,024 us; T5 loaded light p99 33.0 ms against 8.91 ms | `261113-164926-89b9ef` | No mechanism here: `light-share` |
| Zero allocations per query (A7) | After warm-up every class allocates 38 to 123 times per query at the median; the union class up to 411 (21-178 KB); the term class grows with matches to 317 | `261111-072649-debd0a` `a1/log:52-` | Quest 2 A1 attributes the sites first |

**P1, the fault probe** (new, residency `bench/fault_probe.cc`): map a file, evict it
(`posix_fadvise(POSIX_FADV_DONTNEED)`, plus `msync(MS_INVALIDATE)` on FreeBSD), confirm with `mincore`
that no sampled page is resident (exit 2 otherwise), then read 1,000 random 16 KiB ranges on one
thread through a backend's `prefetch`. It prints major faults per 1,000 ranges on the reading thread
(`getrusage(RUSAGE_THREAD)`) and its held time per range. Light jobs in the Linux and FreeBSD VMs:
a fault count is not a timing.

**P2, the stale-verify test** (new, residency `test/stale_verify.cc`, Linux): map a file, verify every
region, rewrite one region with `O_DIRECT`, evict it (checked by `mincore`), read through the map: the
bytes fail their CRC-32C, yet `verify()` returns true. With D6's invalidation on transition it
returns false: `strict-reads` A1 runs it `--fails-without`.

## Decisions

**D1. Residency runs as a `query` chore; completion's drivers wait on a measured miss.** residency
gains `pool_backend<E>`, generic over an executor that posts work and resumes the awaiter; Xapiand
passes its `WorkPool` `query` class, which already serves blob reads and keeps a reserve worker
(phase7c D4). The chore `mincore`s the ranges, populates the absent pages from the worker (D4), and
resumes the reactor. No completion change, no per-reactor queue, ring or fallback thread, no polled
AIO on macOS, no dependence on io_uring being allowed (container seccomp). Its cost: a pool worker
blocked per range set, beside flush and merge. `xapiand-residency` A2 measures it with
`stall_load.py --index-size 2xram` in the Linux VM, reporting flush latency and the merge backlog
beside the fault counts. If the chore backend misses A4, or flush p99 or the merge backlog leaves the
A/A band, the contingent quests 7 and 8 build the completion backend, the same procedure runs on
both, and the loser is deleted: one mechanism. *Lost:* completion first, which nobody had a number
for, and which is the larger, Detent-coordinated change.

**D2. The interface keeps `prefetch`; views are the mapping.** `task<std::size_t> prefetch(std::span<const
std::byte> mapped, std::span<range> ranges)` partitions `ranges` in place so the first n are those it
found absent and brought in, and returns n; it allocates nothing. Every view a reader takes is the
mapping's own contiguous bytes, any range: no read is served from a cache, so no view crosses a
cache boundary (F2's view question). *Lost:* `acquire -> resident`, which exists only to hand out
cache-served views, and each way of making those fit prospect's contiguous decoders: a copy into
scratch per read (allocations, against A7), region-by-region decoders, or region-aligned codec blocks
(a format change).

**D3. The source is the mapped byte span; `residency::file` is deleted.** Its one caller moves
(prospect `tools/prospect_engine.cc`, 10 sites; `test/concepts_test.cc:23`). residency keeps task and
crc32c as its only dependencies. `sync_backend` keeps `madvise(MADV_WILLNEED)` on the span: faultlab
F8's `File::advise` is `posix_fadvise` on Linux only (cpp-libraries `docs/design/faultlab.md:184`) and
advises the file, not the mapping, so one advice path per object: `madvise` for a mapping,
`File::advise` for a file. Runs after `faultlab-seam`.

**D4. How the chore populates.** Linux candidate: `madvise(MADV_POPULATE_READ)` (5.14 and later).
Everywhere: touch one byte per page on the worker. Both copy nothing and leave the reactor no faults,
minor ones included. `residency-api` A3 runs P1 with both on Linux; if equal within the band, touching
is the one path. A page evicted between the populate and the read still faults; A4's 1 per 1,000
allows it, and P1 counts it.

**D5. Check before suspending.** `prefetch` `mincore`s first and posts a chore only when a page is
missing: an index in RAM pays one syscall per range set. On a wide search that repeats per segment
of every index; `prospect-residency-point` A3 measures it at 2,500 indexes (F8). A host flag "assume
resident" (skip the check on a node under no memory pressure; never in strict mode) is added only
if that row leaves the A/A band.

**D6. Strict mode re-verifies on residency transition.** `verified_regions::invalidate(range)` resets
"good" to "unknown" for the regions `prefetch` reported brought in; the readers' existing `verify` on
first touch (prospect `reader.hh:321-326` per range; strata through `section_io.hh:254`) checks them
again. A `verify` racing an `invalidate` must not store a stale "good": each region's atomic state
carries a generation that `invalidate` bumps, and `verify` stores its result with a compare-exchange
against the generation it loaded. Residual: a page evicted and re-read after its verify without
passing through `prefetch`; `strict-reads` A4 states its rate or closes it with `mlock` for the
query's lifetime, by cost. *Lost:* the verified block cache for strictness, which needs a read path
other than the mapping (D2's lost options) where correctness does not. The CLOCK cache stays format
14's decoded-block cache (Q1).

**D7. Nothing per open index; nothing new per reactor.** The chore backend uses the pool's workers,
so T1's census is unchanged (`xapiand-residency` A1). An open index keeps `verified_regions`' state,
about five bytes per region today (`residency.hh:164-170`), D6's generation inside the same byte. If
the contingent completion backend is built, quest 7 states its per-reactor thread, ring and io-wq
workers (F7).

**D8. Scheduling tuning is not a Phase 6 quest.** `light-share` and `wide-search-loop` own it;
`loop-scheduling` showed the remainder is runnable loops off their CPUs
(`docs/design/loop-scheduling.md:32-37`).

**D9. Landing.** Each backend lands behind an option, off by default; defaults flip in
`xapiand-residency` after A4 passes, after the release (D39).

**D10. Gate machines.** Linux VM (strata's, 8 vCPUs, 16 GB): A4, P1, P2, the ratchet rows A5 to A8.
FreeBSD VM: P1 and the tests. A2 cannot be judged in the Linux VM: its unloaded loop lag is p50 640 us,
p99 2,048 us (E3, `261112-141805-74aaf9`) against the Mac's p50 320 us, p99 512 us (`261113-164926-89b9ef`),
and the review's probe reproduces that floor with a bare `timerfd` and nothing else running
(`.scratch/review-phase6-split-design/timer_floor.txt`), so it is the host's HVF timer delivery, not
Xapiand. Q2. This draft did not touch either VM.

## Quests

In the order they unblock. Sizes count contributor sessions and lines changed.

### 1. `residency-api`: prefetch over the mapping, invalidation, and the pool backend

- Repo `residency`; files `residency.hh`, `test/**`, `bench/**`, `docs/**`, `README.md`, `CMakeLists.txt`.
- Risk high (a frozen public API, lock-free state); lens `correctness`. After `faultlab-seam`. Unblocks 3, 4, 5.
- A1 [artifact] The contract, `docs/prefetch.md`: D2's signature and partition, D5's check, D6's `invalidate` and generation, `pool_backend`'s executor concept.
- A2 An executable model enumerating every interleaving of `verify` and `invalidate` on one region: no stale "good" is stored; plus a TSan stress: jobs.
- A3 P1 in the Linux and FreeBSD VMs with `sync_backend` (the baseline) and `pool_backend` (populate and touch on Linux, touch on FreeBSD): the chosen population has at most 1 major fault per 1,000 ranges on the reading thread: light jobs.
- A4 P2 reproduces the stale pass with `sync_backend` in the Linux VM: light job.
- A5 Tests green on Linux, macOS and FreeBSD with ASan, UBSan and TSan: CI plus `campaign verify`.
- Serves A1, A4, A9. Size: 2 sessions, about 600 lines.

### 2. `query-zero-alloc`: no heap allocation per query in steady state

- Repo prospect; files `include/prospect/{searcher,scorer,operators,search,slice}.hh`, `bench/alloc_count.c`, `bench/allocs.py`, `test/**`, `docs/perf/query-allocs.md`.
- Risk medium; lens `correctness`. After `track-p`. Unblocks 3 (shared files) and 6.
- A1 [artifact] The allocation sites by class and command from a steady-state heap profile, the harness's own split from the engine's, and the size restated from it: `docs/perf/query-allocs.md`, light job.
- A2 `allocs.py` reports 0 allocations per query after warm-up on the term, conjunction, disjunction and phrase classes, every command: light job.
- A3 A test that fails on any allocation in a steady-state query on those paths: `--fails-without` on a mutant that allocates.
- A4 Latency on the four classes within the A/A band of `main`: strict job, ABBA.
- Serves A7 (allocations). Size: unknown until A1; 3 sessions is the guess at 38 to 123 sites per class.

### 3. `prospect-residency-point`: queries ask for their ranges before they read

- Repo prospect; files `include/prospect/{searcher,slice,scorer,strata}.hh`, `include/prospect/values.hh`, `test/**`, `tools/prospect_engine.cc`, `CMakeLists.txt`.
- Risk high (the query path the release ships; lands behind an option); lens `correctness`. Needs `residency-api`; after `query-zero-alloc`. Unblocks 4, 5, 6.
- Contract: `prospect::slice` (already the per-request context, Xapiand `src/engine/strata/search.cc:734`) carries an optional type-erased backend; without one, today's behavior. Points per segment: the term-dictionary blocks the query's terms fall in, then their postings (positions for phrase and near) and the value columns in use; stored fetches are a third. Under strict mode, the ranges `prefetch` reports go to `invalidate`, value columns verify per region on read as postings already do (today `values.hh:244-245`, `:275` verify once), and prospect's pinned structures are copies.
- A1 With a backend that records every touched page, a query reads no byte outside the ranges it requested, over every query class; each case also runs under quest 2's allocation hook, with zero allocations.
- A2 Oracle parity and `verify` unchanged with the backend on: jobs.
- A3 In RAM, the backend's cost (D5) within the A/A band of `main` on the four classes and on a 2,500-index wide search (the SR3 harness, `docs/gates/phase7c.md:88`): strict jobs, ABBA.
- A4 Under strict mode, a damaged postings or values region read after a transition is `corrupt`, never a result: a test with an invalidated region whose bytes changed.
- Serves A4, A1 (strict), A5 (no regression). Size: 3 sessions, about 900 lines.

### 4. `xapiand-residency`: the reactors stop faulting at twice RAM

- Repo Xapiand; files `src/engine/strata/search.cc`, `src/engine/host*.h`, `src/opts.cc`, `harness/stall_load.py`, `docs/mechanisms/residency.md`.
- Risk medium; lens `system`. Needs `residency-api`, `prospect-residency-point`. Decides 7 and 8.
- A1 `pool_backend` on the `query` chore class, one per host, none per index; T1's thread census unchanged (`gate7c_rows.sh` T1, `GATE7C_ALLOW` as at 16b520): test plus job.
- A2 **A4**: major faults on reactor threads at most 1 per 1,000 queries at 2x RAM, unloaded and loaded (`stall_load.py --index-size 2xram`), against T6's 330.5 and 110,093; reported beside it, commit-chore p99 and the merge backlog (pending merges and segment count from `/:metrics`, added to `stall_load.py` if absent), before and after: Linux VM jobs. A miss, or flush and merge outside the A/A band, starts quests 7 and 8.
- A3 T4 and T5 at 2x RAM reported beside the in-RAM run (not gated in the VM, D10).
- A4 The default flips on after A2 passes and after the release: E2E green.
- Serves A4, A2, A3. Size: 2 sessions, about 400 lines.

### 5. `strict-reads`: strata's core reads verified on transition

- Repo strata; files `include/strata/core/{reader,section_io,index}.hh`, `test/**`, `docs/mechanisms/strict-reads.md`.
- Risk high; lens `durability`. Needs `residency-api`, `prospect-residency-point`.
- Scope (F3, chosen): strata's own reads (get, scan and stored frames, format 14's documents) through `prefetch` and `invalidate`, and pinned sections as `residency::pinned` copies. The query side of strict mode is quest 3's, because it owns prospect's residency point and decoders and a quest takes one repository; this quest needs it, so strict mode is whole when both land.
- A1 P2 passes with strict mode on and fails without it (`--fails-without`): Linux VM job.
- A2 Under strict mode no checked byte of a pinned section is re-read from the map: test.
- A3 A damaged region is `corrupt_segment` and the segment heals by media-faults' path, not a wrong result: faultfs test.
- A4 The residual (D6) closed with `mlock` for a read's lifetime or stated with its rate, decided by strict mode's cost on all four axes in the Linux VM, both sides' numbers stated.
- Serves A1, A9. Size: 2 sessions, about 500 lines.

### 6. `phase6-ratchets`: the Phase 6 rows on the gate VM

- Repo prospect; files `bench/**` (not the allocation tools), `docs/gates/phase-6-ratchets.md`.
- Risk medium; lens `system`. After `track-p`, `query-zero-alloc`, `prospect-residency-point`.
- A1 Calibration with PISA binding (QG-R28, F13-R45): exclusive jobs.
- A2 **A5** queries at most 0.7x; **A6** indexing at least 1.3x (0.424x Tantivy-st's time at Phase 3, a pass to re-measure); **A7** memory at most 0.7x; **A8** disk at most 0.8x whole and 0.9x per section: gate VM jobs by PLAN's passing rule; a missed row blocks unless the owner carries it (Q3).
- A3 [artifact] The report, every row with ratio, interval, commit and platform.
- Serves A5 to A8. Size: 2 sessions plus about 15 exclusive jobs of 10 minutes.

### 7 and 8, contingent on `xapiand-residency` A2: `completion-read-depth`, `residency-async`

Created only if the chore backend misses A4 or slows flush and merge. Quest 7 (completion): queues
hold N reads, refusing the next without side effects; it states the fallback thread's depth, the
per-reactor thread, ring and io-wq workers at depth times reactors (F7); the lead coordinates it
with Detent first. Quest 8 (residency): a backend on completion `Read` primitives in its own CMake
target, so residency's core keeps no completion dependency (F6). A2's procedure runs on both
backends; the loser is deleted. 2 sessions each.

### Outside the gate path

`host-block-cache` is format 14's step 3 (the decoded-block CLOCK cache, its lock-free design
review). It serves no Phase 6 row under Q1 (a); its M9 row decides whether it ever serves postings
for indexes much larger than RAM.

### The umbrella keeps

`phase6` re-runs A2 and A3 at its commit after `light-share`, `wide-search-loop` and
`xapiand-residency` land, gathers A9 from the quests (no decoder is added; quest 3's values-verify
change gets fuzzing if it touches the decoder), and writes A10.

## Coverage

| Row | Quests | Machine |
|---|---|---|
| A1 async backend and verified reads | 1, 3, 4, 5 (7, 8 if contingent) | Linux VM, FreeBSD VM, CI |
| A2 reactor p99 stall | `light-share`, `wide-search-loop`; re-run by `phase6` | bare metal (Q2) |
| A3 light-query p99 | `light-share`; re-run by `phase6`; 4 reports it at 2x RAM | bare metal (Q2) |
| A4 faults at 2x RAM | 1 (baseline), 3, 4 | Linux VM |
| A5 queries | `track-p`, 6 | Linux VM |
| A6 indexing | 6 (passing at Phase 3; re-measured) | Linux VM |
| A7 allocations per query | 2, 6 | Linux VM |
| A7 serving and indexing memory | **unowned** (Q3); 6 measures | Linux VM |
| A8 disk | **unowned** (Q3); 6 measures | Linux VM |
| A9 every gate | each quest, gathered by `phase6` | CI, VMs |
| A10 report | `phase6` | none |

## Open questions

**Q1 (owner). Reword A1's item?** Phase 6 names "io_uring and POSIX AIO residency backends; the
verified block cache" (strata `PLAN.md:642`). D1 and D6 build one off-reactor backend on the host
pool and verified reads on transition; completion's drivers only on a measured miss; the cache stays
format 14's. Options: (a) reword to "an asynchronous residency backend, chosen by A4's procedure, and
verified reads in strict mode"; (b) keep the wording and build the completion backend and the cache
regardless. *Recommendation:* (a); (b) builds two mechanisms without a measured reason. The
interface change itself (D2, D3) is the lead's: nobody calls `prefetch`, and `residency::file` has
one caller.

**Q2 (owner). Where do the bare-metal rows run?** (a) The development Mac under `quiet exclusive` for
A2 and A3 (unloaded p99 512 us, `89b9ef`), A4 as a count in the Linux VM; (b) wait for a bare-metal
Linux host; (c) first, one probe: the review's `timer_floor` with QEMU at a raised macOS QoS or
timer-coalescing class (`taskpolicy`), under `quiet exclusive`; A2 moves to the Linux VM only if its
p99 is at most 512 us, the Mac's own. Guest-side tuning is out: spinning vCPUs made the floor worse
(p99 3.1-10.6 ms). *Recommendation:* (a) now, (c) as one 10-minute job that could move A2 to Linux.

**Q3 (owner). A7's serving memory and A8 disk have no quest.** At Phase 3, serving memory is 0.94x of
Tantivy against 0.7x (the cache charges bytes, it does not reduce them); disk is 0.993x whole, postings
1.012x and terms 1.123x against 0.8x and 0.9x (prospect `docs/gates/phase-3-ratchets.md:113`,
`:120-122`). Options: (a) name the work now (a format pass for postings and terms, a serving-memory
pass on pinned structures), both crossing into format 13 or 14's open bytes; (b) expect
`phase6-ratchets` to report them as misses for the owner to carry, with the gap and the work named.
*Recommendation:* (b) until the release, then (a) as Track P items; no design that wins these rows
is in hand.

## First buildable steps

1. `residency-api` once `faultlab-seam` lands: its contract, the model, P1 and P2, which give every
   later quest its baseline.
2. `query-zero-alloc` A1 (attribution) once `track-p` releases its files.
3. Q2 (c)'s probe, one exclusive job, whenever the owner agrees.
