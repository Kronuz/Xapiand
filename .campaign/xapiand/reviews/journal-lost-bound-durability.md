# journal-lost-bound review: durability

Commit: journal:ebd3038d1ea58146b14b32459c4aa409e5a61a60 (base: origin/main journal:335b1b5f1e71a2b44ac951363938925abf2ef5f3)
Verdict: CHANGES
Date: 2026-11-21

Reviewer: durability lens, fresh context, for Detent's lead (Detent depends on journal).

## Evidence I relied on

All jobs ran at journal:ebd3038 unless noted. I read their records and did not rerun them.

- 261121-105412-c88261 passed: `lost_bound_test bounds`. It checks the bound for each group by value (groups 1, 2 and 4 at 2), the records kept after the bound, the clear by a checkpoint, and `lose` (lost and refused).
- 261121-105355-e0cfd0 passed: the whole `host_test`, with `idle_damage` checking bounds by value.
- 261121-105415-3c8f01 and 261121-105358-5bfdac failed as required at the mutant journal:63494f8, which loses every named group. A1's `--fails-without` holds.
- 261121-105413-504c72 passed. It runs `lost_bound_test crash`, which cuts power at every mutation of the first open after mid-log damage, then `second`, `retention`, `turn_log_test`, `turn_log_power_test`, the placement damage cases and all 8 remap shards (bounds stay stable through every re-partition crash point).

These jobs establish A1 and A2 for damage that a later complete turn acknowledges. I probed the cases they do not cover.

## What I checked myself

Scripts are in `/Users/kronuz/code/Kronuz/Detent/.scratch/detent/review-journal-lost-bound/`. Rerun with `run_probe.sh <probe.cc>` as a `campaign job submit --quest journal-lost-bound --fresh` job, with `--env MODE=one|two` for the cut probe.

- `multi_block_gap_probe.cc`
  - 261121-141255-ae52c7 at ebd3038: exit 1, the defect.
  - 261121-141353-0853e4 at main: exit 0, group 1 lost.
  - Layout: 1 KiB blocks. Block W holds turn K (group 1, sequence 2), then turn T's first record (group 2, 700 bytes). T's record starts in W's second sector and ends in W+1, followed by group 1's sequence 3 in W+1's first sector. One flipped payload byte in each of the two fragments. T was committed (acknowledged) before the damage.
- `open_cut_probe.cc`
  - 261121-141538-9a63b7 at ebd3038, MODE=two: 11 of 17 cuts disagree.
  - 261121-141627-fe334f at main, MODE=one: 11 of 17 cuts disagree.
  - Layout: the same, with a power cut at every mutation of the first open after the damage, then a reopen.
- Merges, by reading `git merge-tree`:
  - With journal:60ab864 (turn-buffers): one conflict, in `turn_log.h`'s contiguity loop.
  - With journal:e912a28 (tail-second-sector): a CHANGELOG conflict only.
- I/O surface: the diff touches only `host.h`, `journal.h`, `turn_driver.h` and `turn_log.h`. It does not touch `File`, `IO`, `IOOperation` or the reader interfaces. Turn `stage`, `completed` and `flush` are unchanged. `Host::open` adds one table commit through the existing `commit()` (`mark_bounds`, host.h:857) before it serves. `Host::lose` reuses `mark_lost`.
- Detent's own use of journal (grep over `src/`, `include/`): only `IO`, `File`, `IOOperation`, `DirectoryCursor`, `AdmissionLimits`, `MutationCompletion`, `PosixIO` and `drive_synchronously`. Detent never reads `GroupStatus` or `TurnLog`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | **The bound can be too low, and it turns a loud loss into a silent one.** `gap_damage` (turn_log.h:677-683) reads only the manifest of the block where a gap item starts. `scan_segment` merges a gap across blocks (turn_log.h:207, :242): a damaged fragment in W, the orphaned continuation in W+1, then a second damaged fragment in W+1 make one gap item. The records the gap destroyed in W+1 are named only in W+1's manifest, and nothing reads it. A later complete turn's kept-manifest check (turn_log.h:824) catches a lost tail. Past the newest epoch's last kept turn (rule 5, turn_log.h:749-756) nothing does, because those manifests are past `kept_end`. Result at ebd3038: group 1 is `bounded` at 2 with no records after it, while its acknowledged sequence 3 is gone. A reader at 2 is told to continue, and the log's next sequence is 3, so the lost sequence is reused. At main the same layout loses groups 1 and 2 loudly. This contradicts lost-bound.md:15-16: "Every record the damage could have taken is at or below the bound." | 261121-141255-ae52c7 (defect); 261121-141353-0853e4 (main: lost) | Name every block a merged gap covers: walk from the gap's first block through the block where the next item starts (or the segment's end), take each block's manifest, and set `unknown` if any is missing. Alternatively, start a new gap item at each block whose header verifies. Either fix also covers groups the first block does not name, which lose the same record silently at main (pre-existing). Test: add this probe's layout as a `lost_bound_test` case asserting lost, or a bound of at least 3. |
| F2 | no | gap (pre-existing) | **A power cut during the first open after rule-5 damage forgets the damage.** `TurnLog`'s constructor writes the new epoch's open record (turn_log.h:272-274) before `Host::open` stores bounds or losses. After a cut between the two, the damaged epoch is no longer the newest. Recovery then ignores its turns past the open's kept turn (turn_log.h:695-703, :757), so the reopen reports `damaged 0` and both groups `stored`, and sequence 3 is silently gone. The same 11 of 17 cuts disagree at main, so this commit did not introduce it. It does make lost-bound.md:43 false for rule-5 damage: "A crash before the commit leaves the damage in the log, and the next open finds the same bound." The contributor's `crash` case covers only mid-log damage, which an earlier epoch still counts. | 261121-141538-9a63b7 (ebd3038); 261121-141627-fe334f (main) | Narrow the doc sentence to damage a later kept turn acknowledges, and put the ordering on the hunt list: store the verdict before the open record, or count an earlier epoch's damage past its successor's kept turn while the table has no verdict for it. |
| F3 | no | gap | **"Lost loudly" depends on every caller checking the status.** `GroupStatus::bounded` is a new enum value, and `TurnLog::keep` (turn_log.h:319-333) removes the refusal a lost group had. A caller that checks only `== lost` can stage records with no error. strata at its main (index.hh:3362-3410) does exactly that: it treats `bounded` as `stored` and replays the records after the bound onto a head that may be below it. The journal `Store` is loud (journal.h:596). | strata/include/strata/core/index.hh:3362-3410 | Owner's call: (a) keep it as is, and land strata's journal pin bump only with turn-log-parity's tolerant-open, which the Contracts section already implies; or (b) have journal refuse a bounded group's `stage` until the reader calls something like `resume(group, head)`, which throws when `head < bound`. |
| F4 | no | gap | **The merge with journal-turn-buffers (60ab864) has a silent trap.** The conflict is in the contiguity loop, but `live_of` (turn_log.h:779; line 846 of the merged tree) sits outside it. It builds `Live` with 4 initializers, which still compiles against turn-buffers' 5-field `Live{..., bytes : 56}` (no `-Werror`). `bytes` is then 0, so forwarded copies of recovered or kept records would carry 0 payload bytes. | `git merge-tree ebd3038 60ab864` | When rebasing, pass `item.payload.size()` in `live_of`, then run `lost_bound_test retention` (it checks forwarded values) and `bounds` on the rebased tree. With e912a28 (tail-second-sector) only CHANGELOG conflicts. Its detection changes feed gap items, but damage still maps to manifests in one place (`gap_damage`), so the damage reporting stays one rule, and F1 applies there unchanged. |
| F5 | no | nit | The quest's A2 says "a second damage keeps the lower bound". The code keeps the maximum, which is the safe choice (the contributor's ruling). | quest file; `lost_bound_test second` | The lead fixes the criterion's wording. |
| F6 | no | nit | CI (`build-test`, `asan`, `tsan`) has not run at the head (`campaign status`). | `campaign status journal-lost-bound` | It is the landing gate already. |

## Named risks, answered

1. **The bound can be too low (F1).** Records of one group across segments are safe, because the filling manifest carries across a segment rollover. Forwarded copies are safe, because the manifest notes copies at their sequence. A turn whose manifest is damaged gives `unknown`, and every group is lost, which is conservative. A hole after the bound is caught by the contiguity check, which requires the records to start at bound + 1. A lost tail is caught by acknowledged manifests, except past the last kept turn, as F1 shows.
2. **Durable and idempotent, once stored.** Later recoveries name only sequences above the stored bound (`superseded`), and `mark_bounds` only raises it. A cut after the commit and a reopen are covered by 504c72. A cut before the commit fails for rule-5 damage (F2, pre-existing).
3. **Reader semantics.**
   - A reader at the bound continues (documented; the cases are the caller's).
   - A reader below the bound calls `lose`, after which the group is lost and refused (tested).
   - A reader with no checkpoint, such as the `Store`, always loses the group and throws `Corruption`.
   - A log checkpoint exactly at the bound does not count (`upper_bound`), which is conservative.
   - Whether the error is loud depends on the caller (F3).
4. **Merges (F4).** Both merge with small textual conflicts. One trap is silent at compile time (`live_of`).
5. **Unchanged.** The I/O surface and completion ordering are as they were.

## Not run

- CI and sanitizers.
- The rebased tree with 60ab864 or e912a28.
- Placement or re-partition with rule-5 damage.
- Strata's `apply` behavior on a non-contiguous replay. F3 rests on reading `attach_log`, not on a run.
