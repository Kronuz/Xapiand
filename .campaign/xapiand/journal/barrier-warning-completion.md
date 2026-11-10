# barrier-warning-completion: journal

State: waiting on jobs 261109-164012-c04487 261109-164105-67e584 261109-164203-419c11 261109-164205-610d32
Quest: .campaign/xapiand/quests/barrier-warning-completion.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on four jobs at completion:6aee970 (branch head): build-test (261109-164012-c04487), real
storage on the internal SSD and four disk images (261109-164105-67e584), nine revert checks
(261109-164203-419c11), macOS ASan+UBSan of the sync tests (261109-164205-610d32). Job scripts
were passed inline (`campaign job show <id>` has them); copies in `~/.scratch/bwc/`.

## Next

1. When the four jobs pass: fill the Evidence table, write Result, set "ready for review".
2. Ask the lead to push the branch so CI runs the Linux and FreeBSD never-warns run of `sync_calls_test` (the `ci:` part of A1).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|

History: 261109-164106-c56db2 (revert checks, 7 mutants) cancelled before it ran and replaced by
261109-164203-419c11 (9 mutants, adds the option and the fallback return).

## Result

To be written when the jobs pass.

## Rulings I made

- R1 Warning shape: a callback, `PosixIOOptions::barrier_warning` (`std::function<void(const BarrierWarning&)>`), not a counter. strata's host and Xapiand need the volume's name in a log line; a counter carries no name.
- R2 When it fires: after the first `F_BARRIERFSYNC` that returned success (0) on the directory's device, not at construction: a read-only or durable-only backend never warns, and a filesystem lacking the call (fallback to `F_FULLFSYNC`) does not warn. To know that, `SyncCalls::barrier` returns 1 after a fallback flush and `detail::files_sync_devices` returns bool; both are `detail`, and every mutant anchor line in `tools/mutants.py` is unchanged.
- R3 Once per volume per process, keyed by `st_dev`, process-wide (an inline function's static), so millions of indexes on one volume warn once. Apple-SSD volumes stay classified, so IOKit runs once per volume. A throwing callback releases the claim and rethrows: the ordering call fails after its barrier (the caller does not rename), and the volume warns again.
- R4 Classification through IOKit, loaded with `dlopen` (no link change: `CMakeLists.txt` is outside the file set, and consumers link nothing more). Rule: every ancestor in the service plane with "Device Characteristics" has a product starting `APPLE SSD ` and location `Internal`; anything unidentified warns (fails toward warning). Own minimal C-ABI declarations instead of including `IOKit/IOKitLib.h`, whose `MacTypes.h` puts `Point`, `Rect`, `Handle` and others in the global namespace of every consumer. If the lead prefers linking IOKit (`target_link_libraries(completion INTERFACE "-framework IOKit" "-framework CoreFoundation")`), it needs `CMakeLists.txt` in the file set.
- R5 A3's file: `docs/mechanisms/barriers.md` is new (the guarantee, the warning, the option); `commit-barriers.md` keeps the commit protocol and points to it. Not a rename: `AGENTS.md` (outside the file set) cites `commit-barriers.md`.
- R6 Additive API: `make_posix_io` gains a defaulted `PosixIOOptions` (strata needs it to pass the option and callback); `api.h` exports `PosixIOOptions`, `BarrierWarning`, `BarrierStorage`. `SyncFiles`, `OrderFiles` and `PreparationFilePolicy::Stage` keep their signatures and default sequences (the existing `ordering_protocol`, `errno_rule` and `full_flush_option` tests assert them unchanged), so no message to Detent is owed under the contract.
- R7 Test volumes: "once per volume" uses two other mounted local volumes found with `getmntinfo` (devfs and an APFS system volume such as Preboot or VM), opened read-only with `allow_shared_directory`, their sync calls replaced so nothing reaches them. `/` cannot serve: it shares `st_dev` with the data volume (APFS volume group).
