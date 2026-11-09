# manifest-log: journal

State: working
Quest: .campaign/xapiand/quests/manifest-log.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 3a (the log layer) is built and committed at strata:6a8bb32; its jobs run (build-test
261109-112947-1006c3, mutants 261109-112951-b8f62d). Step 3b waits for the lead: Blocked on 1
decides the deleted set it is built on, and the lead may want 3a reviewed alone first.

## Next

1. The lead: rule on Blocked on 1 (the owner's) and 2, and on reviewing step 3a alone.
2. Step 3b: `index.hh` on the log. `m_` becomes the log's state (`files::manifest` retires, and
   `manifest()` returns it); `commit_files` builds the edit from the head and the change list (D3)
   and appends it through `log_appender`, or rolls over by `log_within` (D7); `recover` reads with
   `read_log` and issues the open's one `sync_data` (D6.6, C4); `retain` keeps generations by D8,
   reading older logs with `read_log(..., to)`; `check()` replays the log against memory; the
   live-docs, metadata and field files retire; the allowance holds S only while a rollover is
   forced (C5); `pre_freeze` moves to 0x8000'0008; the writer drops its head on any exception from
   `apply()` (A5's writer half).
3. Step 3c: T4's cases 1 to 9 on faultfs (case 9 the kill between `pwrite` and `sync_data`), T6's
   oracle through the index, `media_test`; the 5-minute ASan+UBSan fuzz of `fuzz_manifest_log_file`
   and `fuzz_manifest_log` at the head marked ready; A9's integration jobs.

## Blocked on

1. **SR3's deleted-set representation (A4, and A3's delete cost): the owner's call, per the quest.**
   Today a decoded segment holds its deleted set as 64-bit ordinals: a bitset decodes to 64x its
   bytes, and a delete edit merges into the whole vector, O(n) (SR2's 8.9 s for 128 one-ordinal
   edits on 20M documents, 10M deleted). Options:
   1. Words when they are no larger than the ordinals (n >= doc_count / 64), sorted ordinals
      otherwise; a bitset record decodes straight to words (1x), a deltas record to ordinals (at
      most 8x its bytes, and a deltas form exists only while it is smaller than the bitset). Memory
      stays within 8x the record either way; deletes into words cost O(k); a merge into ordinals is
      bounded by doc_count / 64. The index converts to its own `deleted_bits` at open, after the
      segment's size check. **Recommended.**
   2. Words always (the index's `deleted_bits`): O(k) deletes and no conversion at open, but a
      deltas body of B bytes allocates doc_count / 8 bytes, which the decoder alone cannot bound
      (doc_count is checked against the segment file only at open).
   3. Ordinals always (today), with an ordered container for O(k log n) deletes: 64x per bitset
      byte stays, plus the container's per-node overhead.
   Step 3a stopped there: nothing on the deleted set changed. The rest of 3a does not depend on it.
2. **prospect's suite (A9) needs a companion change once step 3b lands.** prospect's tests read
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

