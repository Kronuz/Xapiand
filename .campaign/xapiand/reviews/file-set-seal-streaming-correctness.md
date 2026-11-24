# file-set-seal-streaming review: correctness

Commit: strata:7a4284c4cdb2511d9a69613acd73ed6be36b8335 (quest/xapiand/file-set-seal-streaming; test alone at strata:213561d9)
Verdict: APPROVE
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261124-004323-a98c76 (A2): `group_file_set_test` at strata:7a4284c4, exit 0. The new block seals a
  16 x 64 KiB + 7 byte file and an empty one, checks both sealed CRCs against the one-shot
  `crc32c(bytes)`, and checks no `read_at` span over 64 KiB in seal or in recovery.
- 261124-004325-148d8f (A2, fails-without) at strata:213561d9: exit 1 in 130 s, its log's only
  failure `FAIL: a seal reads at most 65536 bytes at a time, read 1048583` (the whole 1 MiB + 7 in
  one read); the CRC, empty-file and recovery-bound checks passed there.
- 261124-004328-8df38e (A1, build-test) and CI (A4) were still running when I wrote this; they are
  the lead's to confirm, not mine.

## What I checked myself

All from the code at the commit; no probe job was needed (scratch dir
`.scratch/xapiand/review-file-set-seal-streaming-correctness/` holds only `.purpose`).

1. Byte identity (risk 1). `crc32c(string_view)` is `Crc32c{}.update(bytes).value()`
   (crc32c@0d5460d `include/crc32c/crc32c.hh:262`), the same incremental state `crc32c_of` feeds
   piecewise (`file_set.hh:152-162`), so the value is the same for any split. The loop reads
   `[0, length)` of the length the set recorded (`it->second.length`, `file_set.hh:71`), never to
   EOF: length 0 allocates an empty chunk, skips the loop and returns the empty CRC (tested);
   exactly 65536 is one read; 65537 is 65536 then 1; a short `read_at` advances by `got`. A file
   grown past its recorded length contributes only its first `length` bytes, as the old
   `read(name, 0, length)` did; a file shrunk below it reads 0 and throws `file_set_damage`, as the
   old `read` did (`file_set.hh:56` vs `:157`).
2. Error paths (risk 2). `crc` is assigned only after `crc32c_of` returns (`file_set.hh:71`), so a
   throw mid-stream records nothing, and a replayed `seal` retries from byte 0 (the `crc` guard at
   `:70` is still empty). PosixIO `read_at` retries EINTR and throws `system_error` on any other
   error (completion@d8b538c `include/completion/posix.h:556-562`). In apply, a `system_error`
   other than ENOSPC propagates out of `apply_entry` (`group.hh:876-881`) and the chore fences or,
   for `file_set_damage`, marks the replica damaged (`group.hh:843-851`): the same handling every
   other file effect gets. The deterministic failure R2 named (a `bad_alloc` of the volume's
   length on every replay) is gone: the seal's only allocation is `min(length, 64 KiB)`.
3. IO abstraction (risk 3). `seal` opens through `open(name, false)` -> `io_->open_existing`
   (`file_set.hh:165,171-176`), the group's IO, as before; the test's `bounded_io` wraps exactly
   `open_existing` and `create_exclusive`, the two calls `file_set` makes. The 64 KiB bound is set
   by `crc32c_of`'s own span, not by the IO, so PosixIO in production gets the bound the test
   checks.
4. Whole-file reads (risk 4). In `file_set.hh` the only remaining `std::string(n)` read is
   `read(name, offset, n)` (`:51-60`); no seal, sync or recover path calls it. Its callers:
   Xapiand's apply reading a chunked command's staged part (`journal-replaces-wal`
   `src/database/shard_group.cc:273`, the part's length, bytes the command needs in memory
   anyway) and test oracles (`test/group/oracle.hh:65-66`, `test/placement/repair_group_test.cc:76-78`).
   Nothing on a seal or verify path reads a whole file-set file.
5. Fails-without (risk 5). Its script exits 0 on a pass, 1 only when `run.log` holds
   `FAIL: a seal reads at most`, and 126 on a build error or any other failure; strata:213561d9
   differs from the fix only in `file_set.hh` (`git diff --stat 213561d9 7a4284c4`), whose `seal`
   there is still `crc32c(read(name, 0, length))` (`:70`), one `read_at` of the whole 1 MiB + 7.
   The job's log shows that check as the only failure, so it fails for the reason it names.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | `crc32c_of`'s short-file error says "a sealed file ends before its length" without the name, and at seal time the file is not sealed yet; the old `read` named the file. | `file_set.hh:157` | Pass the name into `crc32c_of` for the message. |
| N2 | no | nit | `install.hh:112` `read_prefix` is a second streaming CRC32C loop over file-set files (with an optional copy) beside `crc32c_of`; outside this quest's file set and A3's scope (`file_set.hh`). | `include/strata/group/install.hh:112-122` | A later quest could make one of them serve both. |

## Not run

No boundary probe (exactly 64 KiB, 64 KiB + 1, grown or shrunk files) as a job: settled from the
loop at `file_set.hh:152-162`. No fault-injected EIO or short read mid-seal. Not checked: the
suite (A1) and CI (A4) results, and the time a 32 GiB seal holds the chore (it reads the same bytes
as before, now in 64 KiB pieces). Not a session of my own: launched as a subagent, so no session
title applies.
