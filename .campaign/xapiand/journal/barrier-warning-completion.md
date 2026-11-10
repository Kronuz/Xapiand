# barrier-warning-completion: journal

State: ready for review
Quest: .campaign/xapiand/quests/barrier-warning-completion.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: every criterion has evidence at completion:e49f288. Next is the durability review.

## Next

1. Durability review (the lead launches it); fold in findings by id.
2. The lead pushes the branch so CI runs `sync_calls_test`'s never-warns run on Linux and FreeBSD (A1's "never on Linux or FreeBSD" half; locally only macOS runs).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261109-165717-c9c7ba (`completion_sync_calls`: `barrier_warning` through `replace_sync_calls` and `replace_barrier_storage`); real storage 261109-165718-cf1cb0 (internal SSD never warns, APFS/HFS+/exFAT/FAT32 disk images warn once); revert checks 261109-165720-bdd2cd (r1-r7, r9 killed); macOS ASan+UBSan 261109-165721-f8f9e8; Linux and FreeBSD never-warns run is `ci:` once pushed |
| A2 | build-test 261109-165717-c9c7ba (`full_flush_option`: exact sequence `fsync a b c dir, full`; `barrier_warning`: no warning with it on); revert check r8 (option ignored) killed in 261109-165720-bdd2cd |
| A3 | `docs/mechanisms/barriers.md` (completion), with `docs/PLATFORM_FINDINGS.md` "Where the barrier warns" and `docs/mechanisms/commit-barriers.md` pointing to it |

History, at completion:6aee970: build-test 261109-164012-c04487 failed (exit 2): the
`std::function` member made `PosixIOOptions` non-literal, so `dir_trust_test`'s `constexpr`
options stopped compiling (strata's `directory_test` has the same). Fixed at e49f288 (R1). At the
same commit 261109-164105-67e584 (real storage) and 261109-164205-610d32 (macOS ASan) passed;
261109-164106-c56db2 and 261109-164203-419c11 (revert checks) were cancelled before they ran.

## Result

completion:6aee970 and completion:e49f288 on `quest/xapiand/barrier-warning-completion`.

- `PosixIOOptions::barrier_warning` (`BarrierWarningCall`, with `barrier_warning_context`) is called once per volume per process, on macOS only, after the first `F_BARRIERFSYNC` that returned success on the directory's device, where IOKit does not find an internal Apple SSD under the volume. `BarrierWarning` carries `st_dev`, mount point, source and the storage found. A barrier that fell back to `F_FULLFSYNC`, a failed barrier, durable calls and `full_flush_ordering` never warn.
- `full_flush_ordering` already existed (completion:03cef1d era); its exact-sequence test stands, and the new tests show it never warns.
- Cost: on the hot path, one relaxed atomic load per ordering call after a backend's first; one IOKit walk per volume per process. No extra sync call (asserted). Default sequences of `SyncFiles`, `OrderFiles` and `Stage` unchanged (existing sequence tests pass).
- Additive API: `make_posix_io(context, directory, mode, options)`; `api.h` exports `PosixIOOptions`, `BarrierWarning`, `BarrierStorage`, `BarrierWarningCall`. `PosixIOOptions` stays a literal type.
- Docs: `docs/mechanisms/barriers.md` (guarantee, per-call durability table, warning mechanism, limits); `docs/PLATFORM_FINDINGS.md` records for Detent: `F_BARRIERFSYNC` succeeds on APFS, HFS+, exFAT and FAT32 disk images (no `ENOTTY`, so no fallback there), IOKit's keys for this host's SSD and for disk images, and that an APFS volume group's system and data volumes share `st_dev`.
- Open: macOS arm64 internal storage's IOKit keys are unmeasured (a mismatch only warns; CI's macOS arm64 runner is a VM and warns by design, which the seamed tests do not depend on). Linux and FreeBSD run is CI's. The deferred macOS sanitizer gate runs on landing; the targeted ASan+UBSan above covers the new macOS code.

## Rulings I made

- R1 Warning shape: a callback, not a counter: strata's host and Xapiand need the volume's name in a log line, which a counter cannot carry. A plain function with a context (`BarrierWarningCall barrier_warning`, `void* barrier_warning_context`), not `std::function`: options must stay a literal type, since completion's `dir_trust_test` and strata's `directory_test` declare `constexpr PosixIOOptions`.
- R2 When it fires: after the first `F_BARRIERFSYNC` that returned success (0) on the directory's device, not at construction: a read-only or durable-only backend never warns, and a filesystem lacking the call (fallback to `F_FULLFSYNC`) does not warn. To know that, `SyncCalls::barrier` returns 1 after a fallback flush and `detail::files_sync_devices` returns bool; both are `detail`, and every mutant anchor line in `tools/mutants.py` is unchanged.
- R3 Once per volume per process, keyed by `st_dev`, process-wide (an inline function's static), so millions of indexes on one volume warn once. Apple-SSD volumes stay classified, so IOKit runs once per volume. A throwing callback releases the claim and rethrows: the ordering call fails after its barrier (the caller does not rename), and the volume warns again.
- R4 Classification through IOKit, loaded with `dlopen` (no link change: `CMakeLists.txt` is outside the file set, and consumers link nothing more). Rule: every ancestor in the service plane with "Device Characteristics" has a product starting `APPLE SSD ` and location `Internal`; anything unidentified warns (fails toward warning). Own minimal C-ABI declarations instead of including `IOKit/IOKitLib.h`, whose `MacTypes.h` puts `Point`, `Rect`, `Handle` and others in the global namespace of every consumer. If the lead prefers linking IOKit (`target_link_libraries(completion INTERFACE "-framework IOKit" "-framework CoreFoundation")`), it needs `CMakeLists.txt` in the file set.
- R5 A3's file: `docs/mechanisms/barriers.md` is new (the guarantee, the warning, the option); `commit-barriers.md` keeps the commit protocol and points to it. Not a rename: `AGENTS.md` (outside the file set) cites `commit-barriers.md`.
- R6 Additive API: `make_posix_io` gains a defaulted `PosixIOOptions` (strata needs it to pass the option and callback); `api.h` exports `PosixIOOptions`, `BarrierWarning`, `BarrierStorage`. `SyncFiles`, `OrderFiles` and `PreparationFilePolicy::Stage` keep their signatures and default sequences (the existing `ordering_protocol`, `errno_rule` and `full_flush_option` tests assert them unchanged), so no message to Detent is owed under the contract.
- R7 Test volumes: "once per volume" uses two other mounted local volumes found with `getmntinfo` (devfs and an APFS system volume such as Preboot or VM), opened read-only with `allow_shared_directory`, their sync calls replaced so nothing reaches them. `/` cannot serve: it shares `st_dev` with the data volume (APFS volume group).

Lead, 11-09: reviewer probe job 261109-171557-edc6e3 failed on a compile error in the probe itself; its rerun 261109-172201-3af41d passed. Durability review APPROVE at completion:e49f288, N1-N7 non-blocking, filed to hunt.md.
