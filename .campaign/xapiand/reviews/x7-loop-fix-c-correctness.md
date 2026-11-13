# x7-loop-fix-c review: correctness

Commit: Xapiand:e73a0d305
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite 261112-215728-3aa92b at Xapiand:1b3d849 (only a master merge since): the seven RESTORE tests pass.
- Mutants, each read for what it mutates and which value check it names: reverse sub-run 261112-201040-503fe0
  (fails the model's `v` GET), children one at a time 261112-202556-1a371b (`peak > 1`), writes on the loop
  261112-215730-1dc11c (`chores_ms < on_loop_ms * 0.2`), no join before a new numeric id 261112-201042-dc40d0,
  B1 defer-only-the-missing 261112-201043-1d9816 (final `v` of id 7), stale break 261112-201044-0da49c, failed
  slice fails the rest 261112-201045-5f5375 (`failed == 1`), guard keeps inline chores 261112-201043-19e10d, a
  missed write fails 261112-202557-3315c8. Each kills a value assertion, not a count or status alone.
- Ratio job 261112-210723-e38d98 for the 0.2 bound (healthy 0.065-0.095, fan-out off 0.32-0.345).

## What I checked myself

Probe 261112-224203-3c47e4 (exit 0, 28 min), script `.scratch/review-x7-loop-fix-c-correctness/tsan_probe.sh`,
model `.scratch/review-x7-loop-fix-c-correctness/restore_model.py` (the x7-loop-stalls reviewer's model plus
`--shards` and `--readers`):

- TSan build of `xapiand` and `test_loop_yield` at e73a0d305. `test_loop_yield` twice under TSan: 683 checks
  passed each run, 0 sanitizer reports; 5 children at most and 5 at once, largest range 300 with 240 repeated ids
  in a sub-run, 3 deferred in both B1 tests, 1 of 40 failed with the thrown slice, 6 slices against 5 with a stale
  failure.
- 5-shard RESTORE of a 1,500-object stream (repeated ids, updates and patches as barriers, schema changes, failing
  objects) against one object per RESTORE, two seeds, with 2 threads issuing SEARCH and GET on the restored index
  during the RESTORE (2,730 and 2,615 calls, none failed): 0 mismatches in items, GETs and schema; no TSan report.

By reading (handler.cc `apply_shards`/`apply_range`, shard.cc `apply_slice`/`written_keys`/`InlineEngine`):

1. Ordering. A term's objects share one placement entry, so one shard and one sub-run; one child takes a whole
   sub-run (`order[next++]`), so no two children write one shard; children, `next` and `active` run on the loop
   (Counted posts to the host; chores resume through the awaiter's scheduler). Ranges, the numeric-id serial
   splits, deferred writes and batches are each joined before the next starts (`when_all`, then the deferred pass;
   the batch ends with `apply_run`). Matches the serial model; the probe agrees.
2. Errors. Children catch everything, so `when_all` joins all of them before results; `lock_shard` is per slice,
   RAII, so no pin leaks and a stale index is reopened at its release. A missed write's document is intact for its
   deferred retry (`do_replace` takes it by reference and restores blob data on a throw). Exceptions aside from F1
   and N1 give exact counts.
3. Lifetimes. `SliceWrite::doc` is moved out of `run[j]` on the loop into `subruns`, which lives in
   `apply_shards`' frame until every child has resumed after its chore; `Chores::run` has no early return, so a
   worker never outlives the frame it writes. Cancellation is by token, not frame destruction (as the pre-existing
   pull chores already rely on).
4. Shared state. The worker writes only its sub-run's `SliceWrite`s, `at` and `missed_terms` (handed off by the
   chore's completion), and the endpoint's atomics (`stale_`, `applied_`, `modified`); `use_chores` is swapped and
   restored inside the turn, before the turn is released (InlineEngine destructs inside the chore lambda).
   Concurrent readers saw no failure and TSan reported none.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Placement fails more than the serial path would (A5, D68). A pin failure or a `fail`/`close_fail` read error on one shard during placement (a turn that times out busy after `DB_TIMEOUT`, "too many indexes are open", corrupt or database) fails every term of the range still unplaced, possibly hundreds of objects, where the serial path gives each object its own pin and turn wait, so a transient busy fails one object and a stale shard is reopened for the next. | `src/database/handler.cc:2545` (`retry_read` rethrows busy: `read_fate(busy)` is `fail`, `src/engine/error.h:86`), `:2559` and `:2566` (`failed[k]` for all `ask_at`), `:2593` | Send a term whose placement read failed (anything but missing or not_available) to the serial path after the join, in input order, as a miss is (`deferred`), so each object gets its own `replace_document_term`. Test: a seam that makes one shard's `written_keys` throw `errc::busy` once; assert no item failed, every object stored, `deferred` equals the affected objects. Mutant: today's `failed[k] = current_exception()`. |
| N1 | no | defect | A throw after a slice's writes (`finish_write(false, true)` → `schedule_autocommit`, not noexcept) is charged to a write: the last one, though applied, when `at == size`, else the next one, never tried. The writes stand, as `finish_write`'s own comment says of a busy commit. | `src/database/shard.cc:1448`, `src/database/handler.cc:2663`, `:2666` | Catch around `finish_write` in `apply_slice` (log, leave the autocommit to the closing commit), so a throw out of `apply_slice` always precedes its first write. |
| N2 | no | gap | A5 has no evidence row in the journal: its tests and mutants (stale break 0da49c, failed slice 5f5375) sit under A4/N4. | `campaign status x7-loop-fix-c` ("A5 no row") | Tag the two mutants and F1's test `--for A5`. |
| N3 | no | nit | `repeated_in_subrun` counts id-less objects of a 1-shard index (all `NEW_NUMERIC_KEY`, one term) as repeats; only a stat. | `src/database/handler.cc` `seen[shard].insert(term)` | None needed. |

## Not run

- No TSan run of the CLI `--restore` path (inline chores, serial path; unchanged by reading).
- No probe of F1 by job: it follows from the catch at handler.cc:2559/2566 and `read_fate`; its fix's test is the
  proof.
- T4/T5/T12 numbers (A2, A3) and the gate check are not this lens; not rerun.
- No Linux or g++ TSan; CI's.
