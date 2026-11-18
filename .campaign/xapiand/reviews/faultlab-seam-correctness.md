# faultlab-seam review: correctness

Commit: completion:924c6a608e94b561c256f73247b074d807127812
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Mutant jobs at completion:a14893a (924c6a6 changes only an io.h comment and the mechanism note,
  so they stand for the code): preallocate 261117-225514-824e0a, advise 261117-225518-f0c92a,
  map_shared MAP_PRIVATE 261117-225523-3d93fa, link via renameat 261117-225527-89b0bf all failed
  without the override, as required. extended_acl 261117-225533-b70e70 and create_mode
  261117-225538-f6cdcf were still running or queued when I wrote this; build-test at 924c6a6
  (261117-231002-0163c4) was running. Not relied on until they finish.
- The static_asserts in test/map_test.cc pin every F8 signature, `Advice`, `SharedMapping : Mapping`
  with `std::span<char> words()`, and `extended_acl` as `bool`; `defaults()` checks each default
  against a File and an IO that override nothing.
- Linux, FreeBSD and ASan+UBSan are CI's (A5); I ran nothing on them.

## What I checked myself

Scripts in `/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-faultlab-seam-correctness/`
(`build.sh` builds both probes against the completion checkout with CMake).

- 261117-233016-64dae5 (`prealloc_probe.sh`, macOS): each case on a fresh 40 MiB APFS disk image,
  PosixIO in it, the volume then filled to ENOSPC, then a write over the range.
  - control (truncate only): write fails ENOSPC, so the method sees a missing reservation.
  - fresh, and after a 3-byte head: `preallocate(0, 8 MiB)` reserves 8 MiB; the write succeeds.
  - sparse-growth (truncate to 4 MiB, `preallocate(4 MiB, 4 MiB)`): the write succeeds.
  - sparse-inside (truncate to 4 MiB, `preallocate(0, 8 MiB)`): preallocate returns success with
    4 MiB allocated; the write over [0, 8 MiB) fails with ENOSPC. F1.
  - enospc (volume full first, `preallocate(0, 64 MiB)`): throws ENOSPC, size and allocation stay 0.
    F_ALLOCATEALL never reports a short reservation as success here.
- 261117-231454-38bdd3 (`edge_probe.sh`, macOS, ASan+UBSan): link onto a dangling symlink is EEXIST
  and creates nothing at its target; link to itself EEXIST; from a directory EPERM; a name with `/`
  is refused by PosixName (so no cross-directory link exists); a symlink or dangling-symlink source
  links the symlink itself (F4 below). While a file has two names, `open_existing` refuses it, and
  accepts it once one name is removed (documented). preallocate on a read-only handle throws EBADF,
  size unchanged. `map_shared` sees a later `write_at`, a store is visible to `read_at`, and growing
  the file under the mapping is harmless.
- Read: io.h and posix.h diffs, defaults against F8 line by line, `PosixSharedMapping` (munmap in its
  destructor; a failed mmap throws before anything is mapped; empty length maps nothing), `link`
  (linkat with flags 0 on the IO's directory fd, which the IO holds for its life), the Linux,
  macOS and FreeBSD ACL branches against Detent's `acceptable_acl`
  (Detent/include/detent/config_trust_internal.hh:14-58), and `open_file`'s fchmod (posix.h:1134).

Questions asked: (1) signatures and defaults match F8; a backend overriding nothing keeps today's
behavior (new members only; `EntryInspection` stays aggregate with a defaulted field). (2) macOS is
all-or-nothing and ENOSPC throws with the size unchanged; posix_fallocate's error return and
FreeBSD ZFS's EINVAL fall back to the default, as F8 directs; the one false success is F1.
(3) link never replaces, refuses EEXIST including a dangling-symlink destination, stays in one
directory. (4) mapping lifetime and munmap are right; the durability line matches the lead's
accepted ruling. (5) see F2, F3, F7; the mode under umask is exact (test at umask 077).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | On macOS `preallocate` reports success for a range that covers holes below `size()`, and a later write in that range fails with ENOSPC. `reserve` returns true when `end <= size()` and otherwise asks only for `end - size()`, so a truncate-grown (sparse) file is never filled in. io.h:114-116 states F8's guarantee unqualified; the macOS limit is only in docs/mechanisms/io-members.md and journal Ruling 5 (not a lead ruling). faultfs, which counts the whole range as capacity, will model a guarantee the real backend does not keep. | 261117-233016-64dae5 sparse-inside; posix.h:673-674 | Options: (a) on macOS also allocate the holes in `[offset, min(end, size()))`: walk them with `SEEK_HOLE`/`SEEK_DATA` and write zeros into each, so the cost is the unallocated bytes only; (b) narrow io.h's contract to "reserves the range past `size()` on macOS" and make faultfs model that. (b) changes F8's text, so it is the lead's or owner's. Test either way in map_test: truncate a file to N, `preallocate(0, 2N)`, and on a reserving filesystem require `allocated_bytes >= 2N` (a); under (b), require what io.h then states. The probe shows `allocated_bytes` tracks the ENOSPC outcome. |
| F2 | no | gap | `extended_acl` is looser than the `mode_only_acl` check F8 says it replaces: macOS ENOTSUP/EOPNOTSUPP report false where Detent rejects anything but ENOENT; FreeBSD EINVAL or EOPNOTSUPP on the access ACL report false without Detent's `fpathconf(_PC_ACL_EXTENDED/_PC_ACL_NFS4) == 0` check. On ChangeWriter's migration such a filesystem passes a check it fails today. | posix.h:481, 490; Detent config_trust_internal.hh:17, 41-43 | Match Detent (throw or report true on those errors, or add the fpathconf check), or keep and say so in io-members.md. Lead's call. |
| F3 | no | gap | Linux now reports a real POSIX access ACL where Detent's `mode_only_acl` accepts every Linux file, so a migrated ChangeWriter rejects ACL-carrying files it accepts today. Stricter, and stated in Ruling 4 and the note; it narrows Detent's behavior. | posix.h:501 | Owner's call: keep (recommended), or report false on Linux to match. |
| F4 | no | nit | `link` checks nothing about the source: a symlink or a dangling symlink links the symlink itself (flags 0), unlike the open paths' `O_NOFOLLOW` plus `validate_file`. Harmless in a directory the IO owns. | 261117-231454-38bdd3 | Optional: `fstatat(AT_SYMLINK_NOFOLLOW)` and require a regular file, or leave it. |
| F5 | no | nit | Held and named inspections now differ for a file carrying an ACL (`inspect()` sets `extended_acl`, `inspect_entry` never does), so `EntryInspection ==` is no longer an identity test for such a file; inspection_test.cc:119 compares them. No consumer compares them today. | io.h:48-51 | One line on the field: compare identity by device and inode. |
| F6 | no | nit | io.h does not say that truncating below a live `SharedMapping` makes access to the cut pages fault (SIGBUS on PosixFile); F8 says faultfs aborts there. | io.h:119-122 | One clause on `map_shared`. |
| F7 | no | nit | io.h:253 says `create_exclusive` applies "the IO's file mode exactly", but `owner.lock` gets 0600 (posix.h:1134); only the mechanism note says so. | posix.h:1134 | Name the exception in the io.h line. |
| F8 | no | nit | `reserve`'s comment says other failures leave the size as it was; macOS does (enospc case), but ext4's `fallocate` can grow the size per allocated chunk before failing. Not verified: no Linux host here. | posix.h reserve() comment | Drop the size claim, or verify on Linux. |

## Not run

- Linux and FreeBSD paths (fallocate, posix_fallocate, posix_fadvise, the xattr and acl branches),
  FreeBSD ZFS's EINVAL fallback, and the ASan+UBSan suite: CI's.
- The extended_acl and create_mode mutants and build-test at 924c6a6 had not finished.
- map_shared durability on macOS after `sync` (the lead accepted "not promised"); shrinking a file
  under a live mapping (known SIGBUS, not run).
- Disk-full behavior on any filesystem but APFS.
