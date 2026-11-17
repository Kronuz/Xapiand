# restore-free-off-loop review: correctness

Commit: Xapiand:a6f6c9bbcb2e038ee44540f7f2241d4d3494a41b
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The production code (src/database/handler.{h,cc}) is right on every path I probed; the one blocker is
the new test, which aborts under ASan and keeps the quest's CI red. a6f6c9bbc..d57bbcb03 changes only
the journal and docs/perf/restore-stall.md, so CI at d57bbcb03 stands for a6f6c9bbc's code.

## Evidence I relied on

- 261116-152349-798018 (items byte-identity, Xapiand:a85559290, exit 0) and its rerun
  261116-163130-ab4353 at a6f6c9bbc (exit 0): 64 of 64 RESTORE responses identical, base against head.
  a85559290..a6f6c9bbc changes src only in src/lua and opts (master's lua-memory-bound), none of it on
  the RESTORE path, so the first run stands for handler.cc too. What it covers is in F3.
- 261116-163133-9cc384 (build-test) and 261116-163133-2c3c88 (e2e) at a6f6c9bbc, both exit 0.
- Mutants 261116-163130-1875e5 (master's handler) and 261116-163130-006d68 (round-1 handler.cc), exit
  0 each: the two new tests fail without the fix they name.
- 261116-163129-4fff0e (ABBA round 2 at a6f6c9bbc, exclusive, PASS): RESTORE-phase p99 head 1.48/1.49 ms
  against base 10.66/10.03 (bound 2.77); 7,126/7,134 docs/s against 6,733/6,680 (band floor 6,627).
- 261116-163130-b509cb (spin at a6f6c9bbc, exclusive, PASS): head 1 loop turn over 3 ms, 3,057 us; base 26
  turns, longest 29,953 us. This is A1's turn bound.
- CI at d57bbcb03 (gh run 43988404387): ThreadSanitizer, "AddressSanitizer + UBSan" (smoke, load and the
  docs E2E, which restores the sample data), unit under TSan, functional E2E and "Build + unit + smoke" all
  pass. Only "Unit tests under ASan + UBSan" fails (F1).

## What I checked myself

Scripts are in `.scratch/review-restore-free-off-loop-correctness/` in the home checkout. Builds:
261116-165152-4397d0 (head a6f6c9bbc, clang 22, ASan, asserts on) and 261116-165155-fade10 (master
Xapiand:33d69428a, the merged master, release).

- Q1 offsets and sizes, probe 261116-165356-62c527 (`probe.py`, `recompare.py`; base release against head
  under ASan; 154 comparisons of RESTORE response and the stored documents, one and three shards): 154 of
  154 identical once the generated ids (random per run) are masked; no sanitizer line in either log. It
  covers what 798018 does not: repeated keys (top level, `_id`, `_op_type`, `_script`, in a MessagePack
  body), `_id` of every type and of 1 to 70,000 bytes (the error text with them), empty objects, `_op_type`
  variants, script and schema objects (the barrier path), MessagePack bodies, a map key that is not a
  string (500 on both), offset and limit, nested arrays with non-map items, a stream that breaks
  halfway (JSON and truncated MessagePack: the objects before it stay, the same on both), and 16, 300,
  12,000 and 70,000 objects in one request (the last is an array32 header). Reading the code agrees:
  `put` records `_at[idx]` after the pack, so a pack that throws leaves the entry at size 0, which
  `results()` writes as the empty map that `admit`'s placeholder was; an undefined result packs as the
  undefined ext (3 bytes, size > 0) and unpacks to the same type; `record` packs the error map
  (status, type, message) in the old order, and `++_indexed` still follows a resolved id.
- Q2 `reads_stored`: the raw scan matches `find` on every case above that reaches it. A repeated `_op_type`
  takes the last value as `find` did; a key that is not a string throws the same `type_error` at the
  same place; `_init_map`'s dedup (last value wins, in the first position, it edits the raw array) now runs
  in the prepare chore on the same object, and every consumer reads through `find`. Each object is
  prepared by one chore, so no two threads fill siblings. The repeated-key and nested cases are in the
  probe (identical on both).
- Q3 lifetimes and cancel, `cancel.py`, jobs 261116-165401-f96eab (head, ASan) and 261116-165406-b10d6c
  (base): no sanitizer line in any scenario. Client gone mid-body and client gone after the whole body:
  the node stays up, takes the next RESTORE, and what was indexed is committed (base and head stop part
  way: 15,883 and 15,644 of 30,000 after a gone-before-response; the mid-body counts differ, 15,378
  against 368, which I read as the ASan build's speed at the 3 s mark, not a difference in behavior; it is
  one sample). SIGTERM in the middle of a 60,000-object chunk: exit 0, all 60,000 documents there after
  restart, on both. A 200 followed at once by SIGKILL: 30,000 of 30,000 documents after restart, with and
  without `?commit`, on both. By the code: `run.clear()` runs after every round's chores have joined and
  after the last `apply`, and `index` touches `run` again only after the chore resumes, so no object
  state is in use while it is freed. The chore's `[&run]` is safe the way `pull_round`'s `[&]` is: the
  `ChoreAwaiter` lives in the awaiting frame (pool.cc:61), and `cancel_all` drops a queued chore with
  `task::cancelled`, so `run` is then freed by unwinding on the loop. The response is written only
  after `indexer->commit()` (search_views.cc:1722-1738), unchanged; the added checkpoints take no turn.
- Q4 the harness: `restore_stall_ab.sh` runs base and head through the same probe (the RESTORE client in
  its own process in both), the same corpus, the same patched reactor, ABBA, exclusive. Base still shows
  the stall with the client moved (RESTORE-phase p99 10.0-11.3 ms against its idle 1.0-1.4), so the move
  removed the probe's own 8 ms and not the stall the quest is about. Head clears the 2x bound in all
  three A/B runs. Caveats in F2.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | `test_loop_yield` aborts under ASan: the new global `operator new`/`operator delete` (tests/test_loop_yield.cc:92-101) replace only the plain throwing `new` and the plain and sized `delete`, which call `malloc`/`free`. `std::stable_sort`'s temporary buffer allocates with the nothrow `operator new`, which ASan still serves, and frees it through the replaced `delete`: `alloc-dealloc-mismatch (operator new vs free)` at test_loop_yield.cc:96, in `RunningNode`'s startup. "Unit tests under ASan + UBSan" fails at d57bbcb03 (gh run 43988404387, job 133722538590) and at bb063e368 (run 43979013372); master's run of that job passes. TSan's unit job and the Linux build pass, so the test's own checks are not at fault. | CI log above; no local ASan run of the test, since CI's trace names the cause. | Replace the whole family consistently (`new`, `new[]`, nothrow, `delete`, `delete[]`, sized and nothrow) or keep the count out of sanitizer builds (skip the two checks under `__SANITIZE_ADDRESS__`). The fix is confirmed by the CI job "Unit tests under ASan + UBSan" at the fixed commit; the mutants 1875e5 and 006d68 rerun to show the two tests still fail without the fix. |
| F2 | no | gap | The note and journal report only the passing round-2 A/B at a85559290. At a6f6c9bbc, ABBA round 1 (261116-163129-344a10) FAILED A2: head 5,851 and 5,522 docs/s against base 6,629 and 6,532 (band floor 6,435); A1's p99 passed (2.29 and 2.63 ms, bound 2.73, a thin margin). Round 2 (4fff0e) passes at 7,126 and 7,134, and the head's first two ABBA rounds at a85559290 read 7,077-7,126. The dip is in both head arms and neither base arm; the only src change between the two heads is master's Lua cache (src/lua, opts), which this path does not touch, so I read it as host noise, but nothing attributes it, and BASE is still master Xapiand:49a73300d, not the merged master. | 344a10, 4fff0e logs | State the failed round and the rerun in docs/perf/restore-stall.md and the journal's Evidence (A2 rests on 4fff0e; 344a10 is a counted run, not dropped). Optional: move BASE to the merged master for the next ABBA. |
| F3 | no | gap | The byte-identity job covers JSON bodies only, one to three shards, at most 300 objects: no repeated keys, no odd `_id` types or sizes, no MessagePack body, no offset/limit, no stream that breaks halfway, and no results array past the array16 header (65,535 and up is array32). The packer is hand-offset code, so the cases matter. My probe covers them (154 of 154 identical). | 798018, ab4353 vs 62c527 | Keep `probe.py`'s cases in `harness/restore_stall_items.py` (or cite the probe job in the A3 row). |
| F4 | no | nit | `test_a_restores_objects_are_freed_off_its_loop` bounds the loop's frees at 50 per object, between 34 (fix) and 75 (trees on the loop): the allocator-count margin is about 1.5x each way and is the platform's. Linux CI passes; a libc++ or glibc change could move it. | tests/test_loop_yield.cc | Leave, or bound by the difference (fix against mutant) instead of a constant. |
| F5 | no | nit | `results()` still builds the whole response array in one turn when the request ends, as the old code's deep copy did; A1's bound is on 5,000-object chunks, where it is small. A single request of 70,000 objects makes one longer turn; not measured. | search_views.cc:1738; handler.cc:3000 | None needed for this quest; hunt row if wide single requests matter. |
| F6 | no | nit | Pre-existing, same on master and head (probe `one:dup-nested`): a RESTORE object with a repeated key inside a nested object fails with 500 "The map only contains N elements". Not this quest's. | 62c527 raw.json | Hunt row. |

## Not run

- The two new tests under ASan locally (CI's trace is the evidence for F1); a TSan run of my probes (CI's TSan
  unit and E2E jobs pass at d57bbcb03).
- A forced `IndexChores::cancel_all` while the `run.clear()` chore is queued: no seam for it. By the code the
  awaiter resumes with `task::cancelled` and the objects are freed on the loop by unwinding.
- Linux or arm64 timing: the quest's numbers are macOS, as its note says (Limits).
- The mid-body client-gone count on a release head: one ASan sample differs from base by speed (see Q3).

## Round 2 (the fix rounds since a6f6c9bbc)

Commit: Xapiand:319821233548 (the diff 25a3a4850..319821233 in src and tests; b776266cf adds only the journal)
Verdict: APPROVE
Date: 2026-11-16

F1 is closed: `test_loop_yield` replaces the whole new/delete family (tests/test_loop_yield.cc:84-125, every
block pairs with `free`), and ASan ran the test 5 of 5 at this commit (261116-210401-f0ed04, exit 0). The new
production line is one rearm; I answer the three questions on it and find no blocker.

### Evidence I relied on

- 261116-210401-f0ed04 (Debug ASan x5, 319821233, exit 0): F1's fix, and the 5-of-5 pass of the chunk-entry check
  that failed 5 of 5 at the old head.
- 261116-210404-041813 and 261116-210407-7db50a (ABBA, 319821233): trivial GET during a sustained RESTORE, p99 head
  1.49-1.71 ms against base 10.32-10.86 (bound 2.8); p99.9 head 2.31-3.37 against base 19.09-20.32; max head
  4.51-7.56 against base 21.49-23.47; docs/s head 6,956-7,073 against base 6,678-6,749. Read from the job logs.
- 261116-210418-1113ac (spin), 261116-210410-fb14c1 (build-test), 261116-210410-820eb0 (e2e): all at 319821233.

### What I checked myself

- Q1, light lane under sustained RESTORE: no, it cannot wait longer than master allows. By the code
  (handler.cc:2996-2998, task.hh `budget::rearm_light`): the rearm sets `spent_` to 4 and marks the scope light,
  exactly what the chunk's start does (search_views.cc:1702), so a chunk gets at most 3 light slices in its takes
  and 3 in its apply (6, where one budget gave 3 before the takes spent any). A light slice is a requeue at the
  tail of the ordinary posts, at most one slice (200 us) long, and the light lane's rule that a light resumption
  waits behind at most one heavy turn per pass is untouched. The cost is 3 more light slices (0.6 ms) per chunk,
  against about 0.7 s of RESTORE per 5,000-object chunk at 7,000 docs/s: under 0.1% of the loop. On a build where
  the takes spend nothing (release), the rearm changes `spent_` from 4 to 4 and nothing else. A non-stream body
  (the JSON array path, search_views.cc:1714-1722) calls `index` once, so the extra is one rearm per request. The
  null budget (handler.cc:1126, the restore from a file) is guarded. The ABBA numbers above measure it directly,
  at the head and under the same sustained RESTORE as the base: p99, p99.9 and max all stay far under the base's.
  I did not write a probe: the job logs already contain the measurement the suspicion needs.
- Q2, the shared constant: `DocIndexer::STREAM_SLICES` has two users, handler.cc:2997 and search_views.cc:511, and
  `REQUEST_STREAM_SLICES` has one use, the chunk-start rearm at search_views.cc:1702. `REQUEST_LIGHT_SLICES` (8) is
  separate and all four of its uses are budget constructors. Consistent. The test keeps its own copy (N1).
- Q3, skipping the count under sanitizers: sound. It drops only the 50-per-object free bound in
  `test_a_restores_objects_are_freed_off_its_loop` (the one assertion the allocator moves); the byte-identity, the
  results-as-one-buffer bound (200 frees for 2,000 objects, still checked under ASan) and the ASan/TSan runs of
  the request path stay. The bound stays checked where the allocator is the platform's: the release "Build + unit
  + smoke" job runs `test_loop_yield` (fb14c1, exit 0), and the mutants 1875e5 and 006d68 (release) show it fails
  without the fix. The macro covers GCC's `__SANITIZE_*` and clang's `__has_feature`, both asan and tsan.

### Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | `tests/test_loop_yield.cc:423` keeps `stream_slices = 4` as a literal ("REQUEST_STREAM_SLICES") beside `light_slices = 8`; the test already includes `database/handler.h`, so it can read `DocIndexer::STREAM_SLICES` and cannot drift from the constant now that it is public. | file:line | `constexpr unsigned stream_slices = DocIndexer::STREAM_SLICES;`. |
| N2 | no | gap | The rearm at the final apply is caught only where the takes spend slices (ASan, TSan): on a release build `spent_` is 4 both with and without the line, so the chunk-entry check passes without it. No mutant of the line was run. | handler.cc:2996, test_loop_yield.cc:1191 | Optional: a mutant job on the ASan build without the rearm, to confirm 1191 fails. |
| N3 | no | nit | docs/design/loop-scheduling.md:18 says each chunk is light for its first 4 slices; a chunk now gets 4 for its takes and again 4 for its apply. | file:line | One clause in D82. |

### Not run

- A mutant of the rearm line (N2); a heavy-saturated search mix with the head's apply rearm (the quest's A1 is
  the trivial GET, and the extra is 0.6 ms of light time per chunk by the arithmetic above).
