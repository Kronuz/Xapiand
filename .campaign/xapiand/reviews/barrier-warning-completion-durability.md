# barrier-warning-completion review: durability

Commit: completion:e49f288fb1438a96d9a21b59cf0eeaa770fc690d
Verdict: APPROVE
Date: 2026-11-09

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- 261109-165717-c9c7ba (build-test at e49f288, 20 of 20 tests). The default-sequence tests `ordering_protocol`, `flush_target`, `errno_rule` and `full_flush_option` in `test/sync_calls_test.cc` are unchanged from completion:560c288 and pass. They record every `fsync`, plus each `fcntl` with its command (`F_BARRIERFSYNC` or `F_FULLFSYNC`), and fail on any other command.
- 261109-165720-bdd2cd: r1 through r9 are each killed. They cover the fallback and failure classification, the once-per-volume claim, the Apple-SSD silence, the throw-path release, and `full_flush_ordering` being ignored. That is enough for A1 and A2 at the seam.
- 261109-165718-cf1cb0: the real IOKit classifier and the real `F_BARRIERFSYNC`. The internal SSD stays silent, and disk images formatted APFS, HFS+, exFAT and FAT32 each warn once.
- 261109-165721-f8f9e8: macOS ASan+UBSan over the sync tests and the real IOKit walk on a disk image.
- GitHub CI run 42733733939 at e49f288 is green on every platform: freebsd-x86_64, linux-x86_64 gcc and clang, linux-arm64-gcc, linux-asan-ubsan, linux-tsan, macos-x86_64 and macos-arm64. It covers `completion_sync_calls`, the "never warns" half of A1 on Linux and FreeBSD (ordering equals the durable sequence, no warning).

## What I checked myself

Scripts are in `.scratch/review-barrier-warning-completion-durability/` (`run.sh`, `CMakeLists.txt`, `sequence_oracle.cc`, `volumes.cc`, `once_stress.cc`). Rerun them with `campaign job submit --quest barrier-warning-completion --ref BASE=560c288 -- bash .../run.sh`. Probe job 261109-172201-3af41d passed (exit 0). Its first attempt, 261109-171557-edc6e3, failed on a compile error in the probe itself, not in the code under review.

1. **Risk 1, frozen-copy oracle.** I built one scenario driver twice: once against completion:560c288's headers, which is what Detent's namespace-s1-s2 pins, and once against e49f288. The driver records every `fsync` and every `fcntl`, with its command and the file it targets, across these paths:
   - `order_files_and_directory` on all files, on an empty set, on a foreign file, and on a mixed set.
   - `sync_directory`, `sync_files_and_directory` (all and empty), and `sync_files` (all and empty).
   - A barrier failing with `EIO`, `ENOSPC` or `EINVAL`.
   - The unsupported fallbacks (`ENOTSUP`, `EOPNOTSUPP`, `ENOTTY`), each with and without a failing fallback flush.
   - A barrier interrupted twice by `EINTR`.
   - A failing full flush in `sync_directory`.
   - All of the above with the owner file held and released, and again with `full_flush_ordering`.

   Head ran four ways: no callback, a callback on seamed "other" storage (1 warning), a callback on seamed Apple storage (0), and a callback on the real IOKit classifier (0, internal SSD). All four 255-line logs are byte-identical to the base log, including all 19 barriers. The `SyncFiles` and `OrderFiles` primitives (`operation.h`) and `PreparationFilePolicy::Stage` (`image.h`) are unchanged and reach the disk only through these `PosixIO` methods. On Linux and FreeBSD, `order_files_and_directory`, `sync_files_and_directory` and `files_sync` are untouched (`posix.h:249-263`, `posix.h:785-808`); the only change there is that `files_sync_devices` returns bool, and it is called only on macOS.
2. **Risk 3, real classification of every mount on this host** (macOS 24.6, x86_64, T2):
   - The internal APFS volumes (`/`, Preboot, VM, Update, Data) all read `APPLE SSD AP1024M (PCI-Express, Internal)` and stay silent.
   - devfs and autofs (`map auto_home`) classify as "not a disk" and would warn.
   - `/` (`disk1s4s1`) and `/System/Volumes/Data` (`disk1s1`) share st_dev 16777220. Both sit in one container on the same storage, so keying by `st_dev` cannot mix two classifications.
   - The first classification takes 3.7 to 5.0 ms, including the `dlopen`. Later volumes take 0.4 to 0.6 ms each.
   - Before the first classification the process had not loaded IOKit or CoreFoundation (44 images). After it, 304 images are loaded.
3. **Risk 3, "once per volume per process" under contention, TSan.** Eight threads with four backends each, plus two threads sharing one backend, all order on one volume. The run is clean under TSan with `halt_on_error`, three times in each mode:
   - plain: 1 invocation, 1 classification, no failed calls.
   - throw (the first callback throws): 1 throw, 1 failed ordering call, 2 classifications, and exactly 1 warning returned in total, including from a fresh backend after the threads join.
4. **Code read.**
   - The hand-written IOKit and CoreFoundation C-ABI declarations match the SDK signatures (`posix.h:322-334`).
   - Every IOKit object and CF reference is released on every path, including the early returns (`posix.h:230-265`).
   - The volumes mutex is not held while the storage is classified or the callback runs (`posix.h:435-439`), so a callback that re-enters `PosixIO` cannot deadlock, and recursion is cut by `barrier_noted_`.
   - Every failure toward IOKit (no load, no symbol, no match, no parents, no "Device Characteristics", a name over 255 bytes) ends in a warning, never in silence.
5. **Risk 2, the doc.** I checked the per-call table and the opt-in section of `docs/mechanisms/barriers.md` against `posix.h:799-818` and `files_sync_devices`.
   - With the default, nothing is promised after `order_files_and_directory`. On an Apple SSD it orders, and the final `sync_directory` (`fsync` of the directory, then `F_FULLFSYNC`) makes everything durable on storage that honors a cache flush.
   - With `full_flush_ordering`, `order_files_and_directory` is already durable: `fsync` of each file and the directory, then one `F_FULLFSYNC` per device.
   - On a fallback (`ENOTSUP`, `EOPNOTSUPP`, `ENOTTY`), the barrier is a full flush and never warns.
   - On Linux and FreeBSD every call is `fsync`.

   The doc states all of this correctly, apart from note N1.
6. **R4, `dlopen` versus linking IOKit.** I keep `dlopen`. Linking `-framework IOKit` would load the same set of about 260 images into every consumer's process at launch, always. With `dlopen` it happens only when a host sets the callback and a barrier first orders without flushing. Every `dlopen` failure mode ends in a warning, not silence, and the hand-written ABI declarations are correct. I found no defect.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | The `barriers.md` table says `sync_directory` issues "`F_FULLFSYNC` on the directory". The flush goes through the owner file while one is held (same device). The table also says "nothing promised" for the ordering call even where the barrier fell back to a full flush, which is conservative but true only for the barrier itself. | `posix.h:813-814`; `docs/mechanisms/barriers.md:51` | Say "on the directory's device (through the owner file when held)". |
| N2 | no | gap | The doc's cost line ("one IOKit walk per volume per process") leaves out what the first classification does. It runs `dlopen` for IOKit and CoreFoundation, loads about 260 images (44 to 304), takes about 4 to 5 ms on the commit thread, and starts CoreFoundation and the Objective-C runtime in a process that had neither. That matters to a host that forks without exec after its first commit. It happens only with a callback set. | probe 261109-172201-3af41d (`volumes`) | Add a line to "How the warning works", step 4, or to "Limits". |
| N3 | no | nit | `barrier_volumes()` is a function-local static with a `std::mutex`. It is destroyed at exit, so a thread whose first barrier lands during static destruction touches a destroyed mutex. This is narrow: it needs a callback, a first barrier, and process exit at the same moment. | `posix.h:351-354` | Leak it (`static auto& v = *new BarrierVolumes;`), or leave it as is. |
| N4 | no | nit | "The volume warns again at its next barrier" after a throwing callback holds for the backend that threw and for backends that have not ordered yet. Backends that found the volume claimed during the failed attempt never check again. The TSan probe shows the volume still warns exactly once overall. | `posix.h:432`, `posix.h:459`; probe throw mode | Change the wording to "at the next barrier of a backend that has not yet noted it", or leave it. |
| N5 | no | nit | The pre-existing comment on `barrier_unsupported` says "ENOTTY observed on APFS, FAT and exFAT volumes". That contradicts the new finding that `F_BARRIERFSYNC` succeeds on APFS, FAT32 and exFAT disk images. The code is not affected: either answer is classified correctly. | `posix.h:142-143`; `PLATFORM_FINDINGS.md` "Where the barrier warns" | Align the comment with `PLATFORM_FINDINGS.md`. |
| N6 | no | gap | Apple Silicon internal storage's IOKit keys are unmeasured. The expected values are `APPLE SSD ...` and `Internal`; a mismatch would warn on every Apple Silicon Mac, which costs noise, not safety. The doc already lists this as unmeasured. | `barriers.md` "Limits" | Measure on Apple Silicon hardware before strata's host advertises the warning. |
| N7 | no | nit | A throwing callback fails a commit whose barrier already succeeded. It is safe (no rename, the temporary is left for cleanup), but a host's logging error becomes a commit failure, and its exception type, not `std::system_error`, escapes the I/O call. This is the owner's call. Options: keep it (errors never pass silently), or contract the callback `noexcept`. | `posix.h:454-461` | Owner's call; keeping it is acceptable. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Not run

- Power loss on any storage. Durability after each call is argued from Apple's documentation and the code, not measured; Detent's T7 power-cut gate is theirs to rerun.
- Real external or third-party drives (USB, Thunderbolt, internal non-Apple NVMe), Fusion or AppleRAID sets, and network or FUSE volumes. They are classified by reading the code only; the multi-member walk of a Fusion set is unproven.
- Apple Silicon hardware. CI's macos-arm64 runner is a VM, and its passing run exercises only the seamed tests.
- The frozen-copy oracle on Linux and FreeBSD. I did not run it there; those paths are unchanged by inspection and covered by CI's sequence tests.
- Static destruction at process exit (N3) and fork after the first classification (N2) were not exercised.
