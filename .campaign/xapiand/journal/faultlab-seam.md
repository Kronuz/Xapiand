# faultlab-seam: journal

State: waiting on jobs 261117-225504-f6e144 261117-225514-824e0a 261117-225518-f0c92a 261117-225523-3d93fa 261117-225527-89b0bf 261117-225533-b70e70 261117-225538-f6cdcf
Quest: .campaign/xapiand/quests/faultlab-seam.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

completion:a14893a carries F8 (io.h members and defaults, PosixFile/PosixIO overrides, map_test,
test/io_members_mutant.sh, docs/mechanisms/io-members.md). Waiting on build-test and six
fails-without mutant jobs (chained after build-test in the quest slot).

## Next

1. Paste `campaign brief`'s Evidence table once the jobs finish; then ready for review.
2. Lead: push the branch for CI (Linux, macOS, FreeBSD, ASan+UBSan: A5).

## Blocked on

Nothing blocks this step. One open question for the lead (see Rulings 6): macOS durability of
map_shared stores at File::sync.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261117-225504-f6e144 (map_test `defaults()` and static_asserts on every F8 signature) |
| A2 | build-test; fails-without: preallocate 261117-225514-824e0a, advise 261117-225518-f0c92a, map_shared (MAP_PRIVATE) 261117-225523-3d93fa |
| A3 | fails-without: link via renameat 261117-225527-89b0bf |
| A4 | fails-without: extended_acl 261117-225533-b70e70, create_mode (no fchmod) 261117-225538-f6cdcf |
| A5 | build-test 261117-225504-f6e144; CI pending the lead's push |

Local pre-check (macOS, clang ASan+UBSan, not evidence): map_test ok; all six mutants killed.
The mutant jobs run on this macOS host only; Linux and FreeBSD paths get CI, not mutants.

## Result

F8 in completion: `File::preallocate`, `File::advise` + `Advice`, `File::map_shared` +
`SharedMapping`, `IO::link`, `EntryInspection::extended_acl`, create_exclusive's exact mode
(already fchmod'ed; now stated in io.h and tested). How it works: docs/mechanisms/io-members.md.
Finding: storage's macOS fallocate takes a partial F_ALLOCATECONTIG success (0.6 of 8 MiB on
APFS); PosixFile asks CONTIG|ALL then ALL.

## Rulings I made

1. Tests live in test/map_test.cc (the File/IO members suite): CMakeLists.txt is outside the file
   set, so no new suite. A rename to an `io_members` suite is a two-word CMake change for the lead.
2. `IO::link`'s default throws `std::system_error(errc::not_supported)` (F8 says "not_supported"),
   like `lock_owner`, not `std::logic_error`.
3. Default parameters F8 names but does not use carry `[[maybe_unused]]` so consumers building
   with -Wextra -Werror stay clean; types and names are as written.
4. `extended_acl` comes from `File::inspect` (the held fd, the check ChangeWriter makes);
   `inspect_entry` opens nothing and reports false, documented on the member. Opening each entry
   would cost Detent's existence checks an open per call. Linux reports a real
   `system.posix_acl_access` ACL, stricter than Detent's `mode_only_acl` (always true on Linux).
5. preallocate: length 0 and ranges inside the file take the default growth; macOS reserves only
   the growth past size() (F_PREALLOCATE cannot reserve holes); `map_shared` past size() throws
   `std::invalid_argument`; advise with length 0 does nothing.
6. map_shared on macOS: XNU delegates fsync to the filesystem; HFS's fsync pushes only the cluster
   layer, APFS is closed, and only msync(MS_SYNC) is documented to write mapped pages. PosixFile
   does not msync. Making F8's "not durable until sync" hold on macOS needs msync in every sync
   path (File::sync, PosixIO::sync_files*, and the native queues in bsd_completion.h, outside this
   file set), or a `SharedMapping::sync`. Recommendation: leave as is now (Detent's ChangeWriter,
   the one consumer, needs visibility, not durability) and decide when a consumer needs durable
   mapped stores. Lead's call.
