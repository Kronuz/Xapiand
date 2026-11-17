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
