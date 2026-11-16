# sync-at-first-commit review: durability

Commit: strata:a297ba00219c658c2ad75b0bf9e2d5f256e8ad46
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Suite 261112-110230-d89b3f (passed at strata:9e120989) and the mutants killed at 9e120989: open
  261112-110231-eee06e, drop 261112-110234-122749, after 261112-110237-c67ad9, gate
  261112-110241-adb8c7, dir 261112-110244-c9702e. a297ba00 is 9e120989 plus a merge of main that
  touches only `catalog.hh`, catalog tests and docs (`git diff --stat 9e120989 a297ba00`), so
  these stand for the reviewed commit.
- Read at a297ba00: `index.hh` open `:5215-5300`, `commit()` `:952-967`, `commit_files`
  `:4877-4934`, `roll_over` `:4952-4983`, `append_edit` `:4992-5021`, `retain` `:5047-5090`,
  `sync_dir`/`sync_inherited`/`inherited` `:3815-3839`, `drain` `:3871-3886`, chores
  `:2905-2917`, flush cleanups `:2864`, `:4586`, `:4692`, merge cleanup `:3358-3362`,
  `publish_merges` `:3387-3432`, `close` `:649-670`; faultfs's epoch model (diff ab0e26a6..9e120989);
  the A2/A3 tests `test/index_test.cc:258-433`.

## What I checked myself

Scripts under `.scratch/review-sync-at-first-commit-durability/` (Xapiand home checkout), run as
jobs at a297ba00:

- `mutants.sh`, job 261112-112021-f300dc (exit 0: all killed). Two halves of the first commit's
  ordering point that the contributor's mutants do not separate, and the gate predicate:
  `nolog` (ordering point orders the directory, not the inherited log) fails A3's
  `CHECK(broken == 0)` (`index_test.cc:335`): a torn 9000-byte tail under a whole new edit is
  caught. `nodir` (a no-file first commit syncs the log, orders no directory) fails A3's
  `CHECK(lost == 0)` (`:334`) and T4 case 10 (`:216`). `nomanifest` (the gate ignores inherited
  `manifest-*`) fails only A2's sync count (`:291`, `:293`), see N2.
- `merge_cleanup.sh` + `merge_cleanup_probe.cc`, jobs 261112-112453-818e6e, 261112-112723-7badcd
  (setup did not merge), 261112-112818-0938da (exit 1: the defect, B1). An executor that runs
  posted work at once, four segments so a tier merges, the merge's output under ENOSPC, one
  `sync_directory` EIO armed before the first commit after an open: with auto_merge off the first
  commit is fenced (control); with it on, the merge's cleanup sync takes the EIO and the first
  commit is acknowledged (generation 5), and the next call is live. Exit 0 on a fix.
- By reading, Q1: append path stages the inherited log and the directory ahead of the edit
  (`:4994-4995`), clears owed only after the edit's sync (`:5009`); rollover's `sync_dir()` after
  its rename is the inherited sync while owed (`:4973`); ENOSPC on the edit leaves owed set
  (conservative). The gate covers every queue path (`retain` from commit, `reclaim`, chores,
  drain, merge publication `:3427`), with the mark `cur_.next_segment` above every segment name
  on disk. Q2 race: `inherited_owed_` is read and written only under the writer lease (all
  callers hold `writer_scope`; chores run under the lease); no pool thread touches it. Q3: every
  `sync_inherited` runs in `durable()` (`fenced_` set before the throw), and the append ordering
  point runs under `guarded()`, so a swallowing caller (`drain`, `:4692`) still leaves the handle
  fenced; `live()` refuses from then on. Close runs no sync and leaves the inherited state as found.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| B1 | yes | defect | A background merge's failure cleanup runs `ctx.io->sync_directory()` on the merge thread (`index.hh:3361`) while the inherited sync is owed; it neither is the inherited sync nor skips (the F1 ruling's two options). Its EIO goes to `job.error`, which fences only at the next `publish_merges`; `commit()` publishes at `:956` before `maybe_merge()` and `commit_files()`, so a merge that fails after that point (one the first commit's own `maybe_merge` posts, or one still running from a `refresh()`) takes the directory's once-reported writeback error and the first commit's `fsync(dir)` succeeds: the commit is acknowledged over a rename a power loss can revert. The note's "a merge thread's cleanup sync goes to the job's error, which fences at publication, so it never hides an error either" (`sync-at-first-commit.md:99`) does not hold. Xapiand runs exactly this configuration (`auto_merge = true`, host executor; `src/engine/strata/shard.cc:170-173`). | job 261112-112818-0938da (`merge_cleanup_probe.cc`): variant 1 "first commit -> acknowledged" with the EIO taken by the merge; control fenced | Options: (a) the merge thread never syncs the directory: it removes its partial file and reports the bytes in the job, and publication adds them to `unlinked_`, so the owner's next `sync_dir()` (the inherited one while owed) returns them; one sync path, recommended. (b) `merge_env` carries an owed snapshot taken at `schedule()` (owed only goes true to false after open, so the snapshot is safe) and the cleanup skips its sync when set. Test: the probe's shape in `index_test` (four segments, the merge output under ENOSPC, a one-shot `sync_directory` EIO, the first commit throws `index_fenced`); the note's D4 sentence corrected. |
| N1 | no | gap | The F1 test's second case (a failed flush's cleanup, `index_test.cc:420-427`) takes the single-thread flush path, whose plain `sync_dir()` EIO reaches `guarded()` and fences anyway, so it passes under the `dir` mutant; only the drain case kills it. The swallowing cleanup it was meant to cover, the parallel flush's `try { sync_dir(); } catch (...) {}` (`index.hh:4692`), is untested. Covered by the one `sync_dir` rule today. | reading; mutant `dir` killed via the drain case only | Set `flush_threads > 1` (and enough docs for two runs) in that case. |
| N2 | no | gap | The gate's manifest arm has no crash test: A3's gate crash test uses only a dropped segment. faultfs keeps any subset of namespace ops, so a reclaim over an unsynced rename that unlinks the predecessor log without the gate can leave no log at all; today only A2's sync count kills that mutant. | job 261112-112021-f300dc, `nomanifest` | Add `unsynced_rename` to the gate's crash loop (`index_test.cc:345-378`). |
| N3 | no | gap | No test reaches `sync_inherited`'s demoted-log branch, which opens the log by name when a demotion closed it (`index.hh:3826-3828`, review F5's fix). | `git diff ab0e26a6 a297ba00 -- test/` | Optional: one A2 or F4 state with the log demoted before the first removal. |

## Not run

No TSan run (the change adds no cross-thread state; reasoned from the lease). No Linux or macOS
backend timing or real-device crash test; the Linux once-reported directory error is the design's
premise, modelled here by faultfs's one-shot fault. A4's numbers (from 34aa3691) not rechecked.
CI at a297ba00 was still running. GPL sources not read.
