# wbuf-inline review: correctness

Commit: strata:3df7af4ff4ac1534cbebd69d1b54c046ea65f155
Verdict: APPROVE
Date: 2026-11-09

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

Scope: strata:5055169..3df7af4. That covers the lane charge (strata:42fd3ba), chunk growth
(strata:e06a5ee) and the test waits (strata:ccd98d0, strata:28087a6), plus the parts of the branch
they touch: `record::emplace`, `push_record`, `rollback`, `lane_fits` and `flush_documents`.

## Evidence I relied on

- Job 261109-075141-87a30b, build-test at strata:3df7af4 (Release, ctest): passed. It covers
  `lane_charged`, `lane_growth`, `chunk_limit_room`, `pressure_flush_bounded` and `defer_io_test`'s
  identity at this code.
- Job 261109-075208-1546f8, macOS ASan+UBSan at strata:3df7af4: passed, 74 of 74.
- Job 261109-012437-13d8c1, the contributor's mutants at strata:efc5663. The C++ there is
  identical to 3df7af4; only docs and scripts changed since. R, M1, M2 and M3 are killed, so the
  chunk-growth tests fail when the change is broken.
- A11's repeat jobs on the FreeBSD VM. Job 261108-214156-aa0cbd: main at strata:5cb0e5a failed 49
  of 300 runs, every one at `checkout_and_check_in`'s `segment_count(ix) < before`, with logs such
  as "4 segments to 5". Job 261109-012438-75c07d: the branch at strata:efc5663 failed 0 of 300,
  4 at once.
- Not relied on: job 261109-075208-b1cedd (macOS TSan gate at the head) was still running when I
  finished. My own TSan probe below covers the paths this review is about.

## What I checked myself

Probe: `.scratch/review-wbuf-inline-correctness/run.sh tsan 300`, with the source in
`charge_probe.cc` in the same directory. It builds the probe in the job's strata checkout and runs
it. Job 261109-085204-31b428 ran at strata:3df7af4 under TSan: PASS, 0 bad, no ThreadSanitizer
report. Two earlier submissions, 261109-081601-258c10 and 261109-084514-bff9a1, did not compile
(probe-side errors) and do not count.

- **L (risks 2 and 4): a `wbuf::lane` against an exact model.** The lane holds variable-stride
  records of 32 to 1024 B, near the chunk limit included. Every eighth constructor throws. The
  directory limits are 1, 3, 9, 40, 200 and 32,768 chunks, with random cuts back to earlier marks.
  - After every step, `tell()`, `bytes()` (chunks plus directory blocks) and the live-record count
    match the model.
  - The model opens each chunk at `next_chunk(total)`, and when the constructor throws it keeps
    the newly opened empty chunk, as the lane does.
  - `length_error` comes exactly when the model's directory is full.
  - `room_for(k, b)` is exact for one record size: k records fit and k + 1 do not.
  - Constructed records equal destroyed ones.
  - Result: 0 mismatches in 36 runs of 20,000 steps.
- **`next_chunk` bounds.** I checked totals from 0 to 2^46. The result is always a multiple of
  1 KiB, from 4 KiB to 1 MiB after the first 1 KiB chunk, and exactly floor(total/32) to a KiB
  when that falls inside the range. `make_ref` of the last record in the last 1 MiB chunk decodes
  correctly and is not `no_ref`. 0 bad.
- **C (risks 1 and 4): `kv_index` under a random workload.** Values are 0 B to 20 KB, with edge
  sizes 960, 980, 990, 1000, 1010 and 1100 B, so both inline and whole records occur (up to 35
  whole at once). About a third of the transactions are rejected after pushing records. The runs
  cover refresh and commit, `flush_threads` 1 and 2 (`flush_min_docs` 8, so two runs really
  split), and both no reader and two live reader threads. Three seeds give 12 runs. After every
  apply or abandon:
  1. The permit equals `buffered_bytes() + map_bytes()`.
  2. Without readers, `b.bytes` equals the sum over the accepted records in `order` of
     `doc_bytes - stride` for whole records.
  3. With readers, `b.bytes` is at least that sum, since kept rejected whole records stay
     counted.

  After the index is destroyed, `byte_budget::used()` is 0. No mismatch, no leak, no TSan report.
- **W (risk 3): the merge's time to report done.** This uses `checkin_fences`' setup (4
  single-record segments, `segments_per_tier` 2, a 2-thread `WorkPool`), with the create fault on
  every other run. Over 300 runs under TSan on macOS: p50 0.42 ms, p99 3.3 ms, max 4.7 ms, none
  stuck.

By reading the code:

- **Risk 1.**
  - `charge()` recomputes the whole want from state (index.hh:2387). It never releases
    incrementally, so a double release cannot happen.
  - `b.bytes` changes only at the push (`+=`, index.hh:4121-4124). It is restored to the mark by
    `rollback` only when the lane is cut too (index.hh:4036-4041), so it cannot underflow.
  - The `doc_bytes - stride` subtraction is guarded by the static_assert at index.hh:2739. The
    widest whole stride is 40 + `sizeof(document)`, within `sizeof(document)` + 96.
  - `flush()` settles the permit after the frozen buffers are written (index.hh:4490). A failed
    flush keeps both the buffers and the permit.
  - Flush threads only read a frozen, pinned lane. Every `bytes()` or `charge()` call runs on the
    owner while no flush thread runs, because `flush_documents` joins its threads first.
  - `file_sink`'s reservation is a `std::string` member (index.hh:196). It is freed on unwind on
    every path. Its `write` drains before an append could pass `gather`, so it never regrows.
- **Risk 2.**
  - Every record is at most `min_chunk` (`emplace`, plus the static_assert at index.hh:2738).
    Every chunk is at least 1 KiB, so a 1024 B record fits exactly in the first chunk.
  - Chunk offsets stay below 2^20 = `max_chunk`, which fits the 17-bit unit field.
  - `room_for` and `append_n` both size from `bytes_`, which `truncate` keeps exact.
  - A new chunk is counted in `bytes_` as soon as it is taken (write_buffer.hh:404), so the next
    `charge()` sees it.
- **Risk 4.**
  - `append_n`'s bound throws before any state changes (write_buffer.hh:395).
  - `emplace` builds the whole document (`whole_of()`) as an argument, before `append_n`, so a
    `bad_alloc` there leaves the lane untouched.
  - A throw from `operator new` happens before `count_` or `bytes_` move.
  - A throw from `evaluate` reaches `abandon` → `rollback` (index.hh:762-767). That restores the
    lane, `b.bytes`, `order` and `docs`, and `charge()` runs again.
- **Risk 3: a test-timing fix, not a hidden ordering bug.**
  - In both tests nothing posts to the pool after `maybe_merge()`.
  - `WorkPool`'s `Parker::park` has no timeout (threadpool `work_pool.hh:260`), and merge aging
    only matters when flush-class work competes.
  - So a lost wakeup or a stuck merge would never finish, and the 60 s wait would fail too. It
    cannot hide either one.
  - What the spin bound caught is a merge that is still running and later finishes. That matches
    49 of 300 failures on main becoming 0 of 300 with the wait, and W's 5 ms worst case here.
  - `done` is stored with release after `job.output` and `job.error` are written
    (index.hh:3366), and the publish reads it with acquire (index.hh:3380). The 60 s wait does not
    change that order.
  - The failures are on main as well (job 261108-214156-aa0cbd), so this branch did not introduce
    them.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | Two comments say the open chunk is "at most an eighth of the lane". `next_chunk` bounds it by max(4 KiB, lane/32): below 128 KiB a 4 KiB chunk can be up to 4x a 1 KiB lane, and past 128 KiB it is a 32nd. The rest of the doc says this correctly. | test/background_merge_test.cc:553; docs/mechanisms/write-buffer.md:110 | Say "at most max(4 KiB, a 32nd of the lane)". |
| N2 | no | nit | "charged whole the moment it is taken": the chunk enters `bytes_` when taken, but the permit grows only at `charge()` at the end of the apply or abandon. A large transaction takes several chunks before one past-capacity `grow`. That is harmless (grow is for memory already allocated). | docs/mechanisms/write-buffer.md:73; index.hh:783, :807 | Say "charged at the transaction's end, whole". |
| N3 | no | nit | `file_sink` now reserves 1 MiB per flush run up front, even for a one-record segment, and this is not charged to the budget. Untouched pages are not resident on macOS or Linux, so RSS does not move. Under the many-indexes workload, each flush still makes a 1 MiB allocation. | index.hh:196 | None needed; optionally reserve `min(gather, an estimate)`. |
| N4 | no | nit | The push's `assert(doc_bytes >= stride)` is live only in Debug. The static_assert is the load-bearing guard, and C's recomputation of `b.bytes` agrees with the code in RelWithDebInfo. | index.hh:2739, :4122; job 261109-085204-31b428 | None. |

## Not run

- FreeBSD: I measured W on macOS only, and did not rerun A11's 300-run VM job. I relied on the
  contributor's 261109-012438-75c07d (at strata:efc5663, the same C++) and main's
  261108-214156-aa0cbd.
- Linux sanitizers and the macOS TSan gate at the head: they belong to CI and the lead's job
  261109-075208-b1cedd, which was still running.
- Mutants against my probe: L, C and W check the code as it is. Killing mutants is the
  contributor's job 261109-012437-13d8c1.
- `bad_alloc` injection into `kv_index::apply` itself. I covered the throw-safety at the lane
  level (L) and by reading the code (risk 4).
- prospect integration (A7) and the performance criteria (A8).
