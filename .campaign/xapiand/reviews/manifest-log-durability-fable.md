# manifest-log review: durability (fable)

Commit: e0508e9805e612ac78b0475d6db6817a04c716c8 (Kronuz/strata, `quest/xapiand/manifest-log`)
Verdict: CHANGES
Date: 2026-11-10

CHANGES: lands after F1 is fixed and confirmed by a rerun of its probe at the fixed commit. The
protocol itself (one write, one barrier, the ordering point ahead of the edit, the rollover's
rename, the torn-tail rule, the fences past the durable boundary) holds under every crash and
allocation probe I ran; the one defect is a bound the writer stops enforcing under `ENOSPC`.

## Evidence I relied on

- `261109-212058-c999f8` (build-test at e0508e9, Release, 73 of 73). Durability stages in it:
  `lifecycle` (power loss at every mutation of 10,000 crash points plus 2,000 with background
  merges, a second power loss after each mutation of the recovery at every 50th point, `recover
  --to` under power loss at every mutation, T4 case 1's 2,000 rollover and lazy-cleanup points),
  `index` (T4 case 9: an edit written and never synced, 64 seeds), `fields` (a torn tail rolls
  over, never appended over), `directory`, `retention`, `media` (every byte of the log faulted),
  `manifest_log` ("crash images" 2,484 honest images 0 misread, "bit flips" 35,200 flips 0 silent,
  "caller rules", "bounds on read", "allocation failure" 58 points, "appender" over faultfs),
  `apply_atomic` (the commit sweep with the next allocation failing too). A7 (crash images, bit
  flips) stands on it; the log files are unchanged since 4d2a541.
- `261109-212222-4d4b55` (CI's configuration at e0508e9: assertions on, full ctest under clang,
  `apply_atomic`, `cold_segment` and `manifest_log` under g++-16/libstdc++, then `stable_bytes`'
  `noexcept` put back terminates `apply_atomic`): R30's fix and R32's fence under both standard
  libraries (risk 4).
- `261109-212117-5b1128` (31 log and index mutants killed, assertions on), among them "the head's
  replay fences only on damage", "the log's barrier fences only on a system error", "a durability
  fence set only once its message is allocated", "an installed record's live length read from its
  body", "a rollover without its directory sync", "no truncate-back", "a truncate-back without its
  sync", "two records in flight", "the end moves before the barrier", "pieces not ending on a
  sector", "a failure inside replay read as a torn tail", "damage without `to` passes the open".
- journal's framing at the pinned commit (CMakeLists.txt:29, journal 5bbeedc), read from the local
  clone: the fragment checksum covers the header after the CRC (type and length) and the payload
  (`include/journal/framing.h:173`), so a length that lies fails its check or, past the end of file,
  reads as cut; the tail rule is the design's (`:258-262`, `:275`, `:306`): a cut header or fragment
  is damage, an all-zero piece is torn, a torn verdict with a whole record after it is damage.

## What I checked myself

Scripts under `/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-manifest-log-durability-fable/`
(the lead reruns them to confirm fixes):

- `enospc.sh` + `review_enospc_probe.cc` (job `261110-023641-b98c95`, exit 1 = the defect is
  present; exit 0 once fixed). Builds the probe as a target of the checkout: a keyed index on
  faultfs, metadata-only commits until one rollover, then 400 commits with the capacity set before
  each to `used + snapshot_bytes - 8` (room for an edit of a few dozen bytes, not for the 177-byte
  snapshot). Result: 400 commits acknowledged, 0 refused, 0 rollovers, the head log `manifest-130`
  at 18,774 bytes holding 400 edits; the plain open fails with `manifest log: past its bounds at
  generation 387` (the reader's 2 x 128 edits); `open(to=386)` succeeds and discards 144
  acknowledged generations. The first probe version (`261110-023326-d71760`) used a 300-byte
  margin, above the snapshot, and so never met `ENOSPC`; it is superseded.
- `mutants.sh` + `mutants.py` (job `261110-024750-6ae64c`, 5 of 5 killed, CI's RelWithDebInfo with
  assertions; `261110-022840-bb16af` and `261110-023723-c93eff` were runner mistakes of mine, an env
  that zeroed lifecycle's rollover points and a missing `STATE_DIR` for `directory_test`, both
  baselines, not code). Index-level durability mutants the quest's list does not carry:
  - D1, the edit appended before its segments and the directory are ordered (D4): `lifecycle_test`
    and `index_test` abort (exit -6), `directory_test` `:713` `corrupt == 0` fails.
  - D2, the rollover's temporary renamed before it is ordered with its segments (D5):
    `lifecycle_test` (rollover points) aborts, `directory_test` `:655-657` (the sync counts) fails.
  - D3, the open appends over a torn tail instead of rolling over (D6.4): `fields_test:452`
    (`log_name != name`) and `:467`, `index_test:1603`.
  - D4, the open reports a generation before syncing the log that holds it (D6.6, T4 case 9):
    `index_test:133` on several seeds.
  - D5, the fallback's rollover may reuse a generation another log holds (D6.8): `index_test:1159`.
- Ordering, by reading completion's `posix.h` (risk 2), naming each call and what it orders:
  - Linux: `order_files_and_directory` is `sync_files_and_directory` (`io.h:249`): `fsync` of each
    new segment, `fsync` of the directory, so the segment's bytes and its entry are durable before
    the edit; the append is one `pwrite` at E then `fdatasync(log)` (`posix.h:187`), which carries
    the size extension; the rollover is `fsync(tmp)`, `fsync(dir)`, `rename`, `fsync(dir)`.
  - macOS: `fsync` of each file and of the directory, then one `F_BARRIERFSYNC` on the directory's
    device (`posix.h:797-806`, `:157-166`; `F_FULLFSYNC` where the barrier is unsupported or with
    `full_flush_ordering`); the log's `sync_data` is `F_FULLFSYNC` (`posix.h:182-191`), the second
    phase that flushes what the barrier ordered ahead of it; the rollover's `sync_directory` is
    `fsync(dir)` then `F_FULLFSYNC` through the owner descriptor (`posix.h:808-818`), which flushes the
    temporary's bytes and the rename together.
  - FreeBSD: `fsync` everywhere (`file_sync_data` falls to `file_sync`; `sync_files_and_directory`).
  - faultfs models the macOS two-phase shape (bytes staged at the ordering point, durable at the
    next file sync or directory sync, R16), weaker than Linux's and FreeBSD's immediate durability,
    so a recovery that passes the model passes those too.
- Crash points and idempotence (risk 1), by reading `recover` (index.hh:5171-5256) against D6's
  table: the open writes nothing but the log's one `sync_data`, the `.tmp` unlinks (idempotent,
  durable at the next directory sync) and the `to` fallback's rollover, which uses a generation
  above every log's (`next_generation_ = max(.., newest + 1)`, D5 holds it); a second crash during
  any of them leaves the same highest log or the fallback's new one. A torn tail is never appended
  to (`rollover_owed_` from `lr.dirty || lr.end != size`, D3 holds it); the dirty log keeps its bytes
  and leaves through reclaim.
- Risk 4's fixes, by reading: after `roll_over`'s `durable` block every statement is non-throwing
  (`log_stream` is an aggregate over a `string_view`, `optional::emplace` of a trivially built
  class, `noexcept` moves of `log_state`), so the head cannot stay behind a renamed log unfenced;
  `append_edit`'s post-barrier section is inside the `catch (...)` fence; a `std::system_error`
  from `write` with a broken appender is rethrown past the second handler and fenced by `guarded`
  (index.hh:2815-2817); `durable()` and the head's replay set `fenced_` before building their
  message (R32, mutant held in 5b1128). R29's `figures_now` branch (`!dead ? total : dirty ? body :
  record`) is held by the install mutant in 5b1128.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | D7's bounds are soft under `ENOSPC` with no floor: an unforced rollover whose temporary meets `ENOSPC` returns false (index.hh:4929-4931) and the edit is appended (`:4884`); nothing marks a rollover owed and the writer tests the bound only at slack 1 (`:4881`), while the reader refuses past slack 2 (manifest_log.hh:1116). Under sustained `ENOSPC` where edits fit but a snapshot does not, acknowledged commits make the log unreadable by a plain open; only `open(..., to)` recovers, discarding them. Design D8 and journal R5 say the rollover waits one commit; the code lets it wait forever. | job `261110-023641-b98c95`: 400 commits acknowledged (0 refused, 0 rollovers), snapshot 177 bytes, head log 18,774 bytes with 400 edits; plain open `corrupt_index: manifest log: past its bounds at generation 387`; `open(to=386)` discards 144 acknowledged generations | Owner's choice: (a) set `rollover_owed_ = true` in `roll_over`'s `ENOSPC` branch before `return false`, so the next commit is forced and reports space with the index unchanged until a snapshot fits (the log never passes 1x plus one edit; the allowance reserves S only then, C5 as written); or (b) keep the soft bound but refuse the append with `space_error` once `log_within(checked.after, end_after, log_edits_ + 1, 2)` fails, never writing what the reader refuses. Either way `enospc.sh` must exit 0 at the fixed commit and a suite test should hold it (T4 case 7 covers the storage permit, not filesystem `ENOSPC` past the bound). |
| F2 | no | defect | `append_edit`'s broken-appender fence builds its message before setting `fenced_` (index.hh:4966), the pattern R32 fixed in `durable()` and the head's replay: a failed truncate-back followed by a `bad_alloc` in that concatenation leaves the writer unfenced with a broken appender until the next commit, which fences through `usable()`. Nothing is acknowledged in the window and memory stays at E; a crash in it reads the cut tail as damage (fail-closed). | reading; the sweep with burst 1 does not reach it because faultfs's truncate does not allocate | `fenced_ = true;` before the `fence(...)` as in R32; a sweep variant failing the truncate-back's sync allocation would hold it. |
| F3 | no | nit | A short `pwrite` (a disk filling mid-record) is reported as `EIO` (manifest_log.hh:1169) after the clean truncate-back, and `guarded` fences (index.hh:2815-2817), where D4's text reads as if a short count on a full disk reports space like `ENOSPC`. Durability is unaffected (the log is clean at E); availability costs a reopen. | reading | Owner's call: report a short count as `space_error` when the truncate-back succeeded, or keep the fence and align D4's wording. |
| F4 | no | nit | `open(..., to)` at a generation in an obsolete log that reclaim's truncation steps have cut below its snapshot fails as `corrupt_index("no whole snapshot")` (manifest_log.hh:1106), not `generation_not_retained` as D6.8 states; a plain open never picks such a log, which is never the highest. | reading | Map "no whole snapshot" on a log below the highest to `generation_not_retained` in `recover`, or amend D6.8. |
| F5 | no | note | macOS ordering (segment and directory before the edit; the temporary before its rename) rests on `F_BARRIERFSYNC`, which Apple guarantees only on Apple SSDs; completion's `full_flush_ordering` and the barrier warning are the opt-ins. Pre-existing from `main`'s rename protocol; barrier-warning-strata is held by this quest. | posix.h:599-601, :797-806 | none here |
| F6 | no | note | APFS, ZFS and UFS size-versus-data crash semantics are stated unchecked in D4; the tail rule reads non-zero bytes past the last whole record as damage (fail-closed) and zeros as torn, never silently. | design D4; framing.h:258-262 | none; a per-platform run of the crash images when CI's matrix carries them |

## Not run

- No power-loss test on a real filesystem (Linux, macOS, FreeBSD); the ordering is the model's and
  the calls' as read. CI's `linux-asan-ubsan` at e0508e9 was still running; my probes ran without
  sanitizers.
- No combined allocation-failure-plus-power-loss sweep: every fenced point is past the barrier, so
  a crash there adds nothing the model's crash images do not cover.
- The catch order in `append_edit` (a `system_error` with a broken appender) was settled by reading,
  not by a probe.
- The other durability reviewer's review and probe job (`261110-022246-d64be6`) were not read.
- A9's integration job (`261109-212247-2389ea`, exit 0) is outside this lens.

Tokens: ~150k
