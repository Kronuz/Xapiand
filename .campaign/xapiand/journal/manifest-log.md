# manifest-log: journal

State: working
Quest: .campaign/xapiand/quests/manifest-log.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 3b (the index on the log), second contributor. At strata:9529bd1: the mutants job
261109-112951-b8f62d at strata:6a8bb32 failed only because the "damage read as a torn tail" mutant
did not apply (its text was the one-line form; the reader's check is multi-line), fixed and renamed
"damage without `to` passes the open". Uncommitted in the worktree, being built:
- D42 in `manifest_log.hh`: `log_deleted` (words from doc_count / 64 deleted, sorted ordinals
  below; delta bytes kept; `insert` walks only to each new ordinal's old neighbours), the decoder
  choosing the form from the count before allocating, the "deleted sets" stage, four mutants.
- `check_edit()` (D3's checks plus the `log_shape` an edit leads to, nothing changed) and `apply()`
  as check-then-change, so the writer decides D7 on the post-edit S before writing.
- `index.hh` on the log: `m_` is the head (`files::log_state`), the live scalars are `cur_`,
  the change list (`deleted_since_` from apply_pending, `meta_since_` with undo),
  `commit_files` = edit_now/check_edit/append_edit or state_now/roll_over, recover per D6,
  retain per D8 (`kept_files`), install via `read_generation`, check() replays the log,
  allowance per C5, demote/close drop the log's descriptor. Not yet compiled.

## Next

1. Build and pass `manifest_log_test`; commit D42 (with its stage and mutants) alone, then
   check_edit.
2. Compile `index.hh` (core_kv_test first), then port strata's tests that read the old files
   (about 94 sites: index_test 32, files_test 29, retention 9, format14 7, lifecycle 7, directory
   6, fields 6, reclaim 6, cold_segment 5, warm 5, background_merge 5, apply_atomic 3,
   reclaim_bg 3, reclaim_backlog 3, media 2, defer_io 1). Commit the index on the log.
3. Retire `files::manifest`, live-docs, metadata and the field table's file codecs (files.hh,
   fields.hh), their tests and `fuzz_files`; `pre_freeze` to 0x8000'0008 (D11). Own commit.
4. Step 3c as before (T4 cases on faultfs, media_test, fuzz at the ready head, A9 jobs).

## Blocked on

1. **Resolved by the lead (L1): a prospect companion quest after manifest-log.** prospect's suite (A9) needs a companion change once step 3b lands. prospect's tests read
   the old files: `format13_test.cc` rewrites manifests through `files::manifest` (about 30 sites),
   `reclaim_test.cc` decodes manifests and live-docs and metadata files, `media_test.cc` damages
   `fields-*` offsets, `fixtures_test.cc` pins `files::pre_freeze`, `directory_test.cc` plants
   `meta-1.kv`. They move to the log in prospect, outside this quest's repository. Recommendation:
   the lead opens a prospect companion quest (or branch) to land with step 3b; the A9 job runs
   prospect at that branch.

## Evidence

| Criterion | Evidence |
|---|---|
| A10 | D41 (PLAN.md, 2026-11-09): Q1 the manifest log first, Q2 a count of generations with `fallback()` reporting the damaged generation, Q3 accepted; folded into `docs/design/manifest-log.md` at strata:49ab291 |

Step 3a's checks, which A3, A5, A6 and A7 build on (rows when the index runs on them, step 3c):
`manifest_log_test` stages "caller rules" (A6), "crash images" and "bit flips" (A7), "bounds on
read" and "replay cost" (A3 without the delete cost), "allocation failure" (A5, the open's half),
"appender" (A1's log writer), in the build-test check 261109-112947-1006c3; 19 mutants in
261109-112951-b8f62d (a local run killed 18 of 19; the survivor was a mutant that did not remove
the damage check, rewritten).

## Result

Not ready for review. Step 3a, the log layer, at strata:6a8bb32 (index.hh untouched):

- `read_log` (D6.2 to D6.4, D6.8): journal's scan, then D6.3's caller rules, replay within twice
  D7's bounds (SR2-strata), `to` with damage past it reported, `log_damage` naming the damaged
  generation; any exception drops the partial state (SR4, the open's half).
- `log_appender` (D4): one write per append in sector-aligned pieces, the truncate-back and its
  sync, one record in flight, a broken state after a failed truncate-back or barrier.
- `apply()`: I13 against an identity set the state keeps, names against the edit's own set.
  `field_table::append` grew its vectors by one per append (quadratic); it grows geometrically.
- Measured locally (macOS, RelWithDebInfo): 2,484 honest crash images over 40 logs, 0 misread (497
  whole, 809 torn, 1,178 previous clean); 35,200 single-bit flips, 0 silent, 40 in padding; ten
  times the declarations in one edit 14.6x the time (118x before the fix); edits adding segments
  onto a hundred times the state 1.6x; 58 allocation points, each bad_alloc, none misreported.
- Design: C4 to C6 and D41 folded in (strata:49ab291).
- Docs: strata `docs/mechanisms/manifest-log-file.md`; `test/manifest_log_mutants.py`;
  `fuzz/fuzz_manifest_log_file.cc` with 48 seeds.

## Rulings I made

- R1 (scope): step 3 does not fit one review: `index.hh`'s commit, recovery, retention, `check()`,
  install and the retired files touch about 160 lines that name the manifest, live-docs, metadata
  and field files, plus about 90 test sites in strata and prospect's tests that read the old files.
  I split it into 3a (the log layer, reviewable alone), 3b (the index on the log) and 3c (the crash
  suite and integration), each its own commits, as the lead's prompt allows.
- R2: merged strata `main` (strata:3df7af4) into the branch rather than rebasing, so the
  reviewed design commits (strata:1c8ff50, strata:79beaf4) the reviews cite stay reachable.
- R3: `field_table::append` reserved size + 1 per append, which libc++ honors exactly, so each
  append copied the table: quadratic in a log replay and in the index's own declarations. It now
  grows geometrically, keeping its strong guarantee (reserve before any change). Shared code inside
  the file set; the "replay cost" stage and a mutant hold it.
- R4: F, R and c are constants, not options; the writer rolls over past 1x D7's bounds and the
  reader refuses past 2x, leaving room for a log written under a larger preset.
- R5 (C5): of the re-check's two fixes I took the mechanism ones: the replay bound scales with S
  (max(R, n, S / c), c = 512) and the allowance holds S only while a rollover is forced, D7's
  bounds waiting one commit under ENOSPC. Written into D7 and D8.
- R6: `open(..., to)` reads to `to` and reports damage after it instead of failing, so a damaged
  log still opens at a whole generation before the damage (D41's Q2); without `to` damage is
  `log_damage` with the generation.
- R7: `log_state` keeps its segments' identities as a derived set (rebuilt by `recount`), so I13
  costs the edit; equality stays a function of the segments.
- R8: the sanitizer fuzz runs (both log targets, 5 minutes each) wait for the head marked ready,
  as the prompt asks; the replay of their corpora runs in every build.
- Lead's rulings, 2026-11-09:
  - L1: Blocked on 2 yes: the lead opens a prospect companion quest after manifest-log that repins
    strata and moves prospect's tests to the log (the list under Blocked on 2 is its scope); A9's
    jobs run prospect at its branch. This quest does not edit prospect.
  - L2: step 3a gets no separate review; one durability and one security review at the head marked
    ready. The decoder's fuzz targets (`fuzz_manifest_log`, `fuzz_manifest_log_file`) stay ready
    for the security reviewer.
  - L3: Blocked on 1 is with the owner (the lead recommended option 1). Step 3b continues on
    everything that does not touch the deleted set; the deleted-set change lands in its own commit
    when the answer arrives.
- Owner's ruling D42 (PLAN.md, 2026-11-09), formerly Blocked on 1 (SR3's deleted-set
  representation, A4 and A3's delete cost): option 1, an adaptive deleted set, bitset words when
  deleted >= doc_count / 64 and sorted ordinals otherwise; a bitset record decodes to words, a
  deltas record to ordinals (memory at most 8x the record); the index converts to `deleted_bits`
  at open after the size check.
- R9 (D42's cost): a delete edit's walk to each new ordinal's old neighbours (for the delta bytes
  S needs exactly, C7) covers disjoint stretches, so it costs O(k log n) on ordinals and at most
  the set's words once on words, the same order D42 accepted for a merge into ordinals.
- R10 (change list): strata has no commit of a past generation yet (durability-modes D1's
  checkpoint is K2's), so the change list holds the changes since the head without marks; the
  edit is always head to current state. Marks come with the first caller that commits a past
  target. T6's past-target cases stay at the log layer (diff/apply round trips).
- R11 (head memory): the writer keeps the head as a full `log_state`: its deleted sets (at most
  the segments' bits again, by D42's forms), metadata map and field table duplicate the index's.
  Sharing the table's prefix and the metadata is a follow-up if T9 or a heap row shows it.
- R12 (retention): a predecessor generation whose log ends before it (a generation a torn tail
  skipped, D6.7) names nothing and does not block; a pin that cannot be read, or any other read
  failure, blocks the round as `main` does.

