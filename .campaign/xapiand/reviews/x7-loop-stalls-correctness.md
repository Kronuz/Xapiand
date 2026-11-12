# x7-loop-stalls review: correctness

Commit: Xapiand:20fc9e13302fb6e0628e10afed4b6dbdc61f89e5
Verdict: APPROVE
Date: 2026-11-12

Scope: the early increment of the quest's 11-12 ruling, which covers the hits checkpoint, compress-once, RESTORE's
prepare as chores with the D1 barriers and D2 input-order commits, `DocIndexer` taking `engine::Chores&`,
`engine::sync_wait` in the chore with its allowlist row, the prospect pin at b20e7c5 and the phase7c.md D4/D12
amendment. The timing rows (A2, A3) are not judged here. Code read at this commit, with concurrency questions
included. No GPL sources were read.

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite 261112-011615-3ea00b at Xapiand:232b81cba, light, exit 0. Its configuration has asserts on and tests on: 38/38
  ctest including `test_loop_yield`, `async_check --strict` (unclassified 0) and the removed-options check. Commits
  from 232b81cba to 20fc9e133 change only the journal and `docs/perf/x7-loop-stalls.md` (`git diff --stat`).
- Mutants. I opened each job with `campaign job show` and read its log.
  - 478825, 05438d, 0c957d, 5c5bae, 86670c and b2314b, at Xapiand:6ff9db94a, each require test exit 1 and their
    named `test_loop_yield.cc` line in the output (`test "$rc" = 1; grep -q ":NNN:"`). Each log shows that line failing.
    - 478825 fails :320 (hits; the worst probe wake was 262 ms against a 245 ms request).
    - 05438d fails :341 (400 compressions against 200).
    - 0c957d fails :415 (`/barrier/a` lacks `second`).
    - 5c5bae fails :414 (`/boundary/c` lacks `first`).
    - 86670c fails :447 and :451 (object 1 accepted where the sequential run fails it).
    - b2314b fails :383 (the loop took 834 of 1,366 ms of CPU).
    - Between 6ff9db94a and 232b81cba, `src/` changes only `task::sync_wait` to `engine::sync_wait` and its include
      (`handler.cc`). The pin and the allowlist row also change. Each mutant's target line is untouched.
  - 0ecdb6, 037ffb, 0e58e3 (a2dffbd5e) and 55273a (0c99ca2bd) also check their line; their logs show :304, :325, :401
    and :304.
  - af5357 and c4c72b (102d6609e) require only exit 1, but their logs show the named checks :237 and :258. 74a222
    (00cd38f39) requires only that the test fails. All three are superseded by the line-checked runs above (see F4).

## What I checked myself

Probe job 261112-021740-4d8c34 at this commit (light, exit 0). Scripts:
`.scratch/review-x7-loop-stalls-correctness/tsan_probe.sh` and `restore_model.py`.

- The build is clang 22 with `-fsanitize=thread`, in its own build directory and its own dependency build trees
  (`.deps-tsan`, sources copied, `FETCHCONTENT_FULLY_DISCONNECTED`).
- `test_loop_yield` ran twice under TSan: 48/48 checks each time and no reports. Each RESTORE test runs `threads-1`
  prepare chores per round on this host.
- `restore_model.py` ran under TSan with seeds 7 and 11 and found 0 mismatches and no reports.
  - The oracle is a sequential model: one RESTORE of 1,500 objects is compared with the same objects restored one per
    request. One shard; about 385 ids repeated across the stream.
  - Fields first appear with integer, string or boolean types all along the stream, so schema changes keep coming and
    there are about 1,040 type-conflict and patch failures per seed. About 6% of objects are updates of recently
    indexed ids and about 3% are patches; both are barriers.
  - Every object's result, every id's GET (including `_version`) and every field's schema entry are equal.

Answers to the lead's questions, from code at this commit:

1. **Ordering and visibility.** Nothing leaves input order.
   - Applies run in input order: `apply_range` runs serially, and the next range starts only after `when_all`
     (`handler.cc:2566`).
   - Index numbers, versions and new numeric ids are assigned in input order. `_idx` is admitted in `take`, in stream
     order. `_version` and `NEW_NUMERIC_KEY` are set in `finalise_write` in the shard's turn.
   - D1 is closed. `reads_stored` makes a barrier of any non-`index` `_op_type`, a root `_script` or `_schema`; that
     covers every input that reaches `call_script` or `get_schema(&obj)`. A barrier calls `apply_run` before it is
     read (`:2586`), and every batch ends with `apply_run` (`:2604`). No prepare runs ahead across a batch.
   - D2 is closed.
     - `round.changed` keeps the lowest index whose prepare changed the schema (a CAS loop over the min).
     - Every lower index was pulled before it, and was prepared against the snapshot, because `changed` never drops
       below the final minimum.
     - `accept` offers only that object's change, against the round's snapshot (`:2533`); then the next fetch
       re-prepares every later object.
     - A failed prepare never offers a change: `Schema::index` resets `mut_schema` on any throw (`schema.cc:2992`). A
       chore returns at its own first change, so the `Schema` it reuses carries no earlier object's change.
     - Per-call state (`map_values`, `specification`) is reset at the top of `Schema::index`, and the body is `const`
       there.
     - Re-prepare is idempotent: `take_op_type` erases only `"index"`.
   - The turn assertion seen at a2dffbd5e is fixed. `fetch` (`:2476`) and `accept` (`:2559`, `:2568`) run only outside
     `when_all`. The only thing beside `apply_range` is `pull_round`, which awaits chores and takes no turn.
2. **Threads and lifetimes.** A chore touches nothing loop-owned or shard-owned.
   - What a chore touches:
     - its own `Schema` over the locked snapshot;
     - its own `run[j]`, disjoint from the `[at, stop)` that the loop applies; `run` is never resized while chores
       run;
     - `placement`, a plain `DatabaseHandler` used only for `endpoints[n].is_active()` and size, since `seq` is
       never 0;
     - the body's own MsgPack.
   - The body MsgPack was moved out of the batch array. It shares the parent's zone, but neither `erase` nor `_fill`
     allocates from it or touches the parent (msgpack 88aab56 `msgpack.h:1494-1531`, `:1869-1898`). TSan agrees.
   - `run`, `Round` and `placement` live in the frames that `when_all` keeps alive (D3's shape).
   - A queued chore cancelled at shutdown resumes with `task::cancelled`, and a running one is drained
     (`pool.cc:118-213`).
   - A client that goes away mid-round throws at `apply_range`'s checkpoint. `when_all` lets `pull_round` finish
     first, and what has applied stays.
   - A throw in one object becomes that object's result (D7 closed, `:2499-2503`).
3. **`engine::sync_wait` in the chore.** It is safe on this path; no plain-object input suspends it.
   - `Schema::index` awaits only in the `seq == 0` shard-count scan (`schema.cc:2768-2785`, `:2843-2860`) and in
     `call_script` when `specification.script` is set (`:2912`).
   - `seq` is `idx + 1`.
   - Only RESERVED_SCRIPT sets the root script: `process_script` from the object (a barrier) or `feed_script` from the
     schema. `fetch` routes the whole run to the loop when the schema has one (`:2478`), and `own` reads the same
     snapshot.
   - A `_script` inside an `_id` map or a nested field never reaches `call_script`. `_get_data_id` reads only
     `_type`, `_slot`, `_prefix` and a few flags.
   - The suite's asserts-on run takes the chore path; `check_may_block` would abort on a reactor thread.
4. **The hits checkpoint.**
   - On cancel, or when the client has gone, the checkpoint throws and no response is written.
   - Yielding between hits adds no new kind of interleaving: each hit's `lock_shard` pin and its blob read already
     await (`search_views.cc:2027`, `handler.cc:2757-2766`). It widens an existing window (F3).
   - Hits are appended in mset order, one at a time, so the response stays well formed.
5. **Evidence.** See above. Every mutant exits 0 only when its test fails. The line-checked ones fail on their named
   line.

Design review D1-D8: D1, D2 and D7 are closed in code and by test. D3 is closed: state lives in the awaiting frames.
D4 is closed: there is a `Schema` per chore over the locked snapshot. D5 is closed: `PREPARE_SLICE` 1 ms, one cursor.
D8 is closed: the `prepare_plain` comment is accurate. D6 is the owner's and is not built.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | `prepare_inline` never clears `one.error`. In `apply_run`'s tail (taken when a later `fetch` sees a schema script), an object that a chore failed in an earlier round, and that `accept` stopped before, keeps that stale error even when its inline prepare now succeeds. This is reachable only when another request adds a root `_script` to the schema mid-RESTORE. | `handler.cc:2380-2386`, `:2570-2574` | `one.error = nullptr;` at the top of `prepare_inline`. |
| F2 | no | gap | Two failures that used to be one object's result now fail the whole RESTORE request: a `get_schema` failure in `fetch` and a throw from `schemas->set` in `accept`. Before, both sat inside the object's `co_catch_http_errors` (master `index_one`). Example: a foreign schema that cannot be read. Applied objects stay, as before. | `handler.cc:2477`, `:2533` | Owner's call (an error-shape change): keep it, or catch both in `accept` and `fetch` and record them as the first pending object's result. |
| F3 | no | gap | The hits checkpoint lets writers commit between hits more often. A hit deleted after the search then fails the whole search through `get_data`'s throw. The window already existed through the pin and blob-read awaits. | `search_views.cc:2027-2033` | Owner's call: keep it, or skip a hit whose document is gone. |
| F4 | no | nit | Three early mutants (74a222, af5357, c4c72b) do not check their named line in the command. Their logs show it, and line-checked reruns supersede them. Separately, 5c5bae's batch-boundary mutant drops each batch's last run instead of reordering it. It still fails on :414, but it also fails on unrelated checks (:319, :341, :346, :382). | job records | Cite only the line-checked jobs in A4's row. |
| F5 | no | nit | With prospect pinned at b20e7c5 (which pins strata 94480d5), the "newest pin" comment at `CMakeLists.txt:456` is false until a strata bump. The build resolves strata at 5d742b2 and the suite passes. The journal already raises this with the lead. | `CMakeLists.txt:456`, `:772` | The lead's strata-bump quest. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Not run

- Any timing, and A2/A3.
- The CLI `--restore` path (`engine::inline_chores()`, nested `engine::sync_wait` on the main thread): read only, not
  run.
- RESTORE into more than one shard under the model. The probe and the tests use one shard. Placement is
  `seq % n_shards` as before.
- Linux and g++ TSan, and the CI matrix: no CI run exists at this commit.
- The two-writer overlap on `docs/design/phase7c.md` with x7-gate: that is `campaign check`'s, for the lead.
