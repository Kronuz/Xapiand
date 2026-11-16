# faultlab-design review: design

Commit: cpp-libraries:a2c60c7193cb6a1637868dd674a082481084c216 (docs/design/faultlab.md)
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape holds: one crash model on completion's seam, one scenario line, one event log, one
Python checker, optional Linux labs. Four claims the migration rests on do not hold against the
code. Two lose crash coverage when the old models are deleted. One makes Detent's step impossible
as planned. One contradicts itself in the text the owner is asked to rule on.

The lead's question numbers map to the doc's as follows: the lead's F2 is the doc's F3 and F5, F3
is F4, F6 is F8, F8 is F10, and F11 is F16 with the migration order.

## Evidence I relied on

- No contributor jobs. The criteria are artifacts, so the evidence is the doc read against the
  cited commits: strata:491a1bd, journal:8f3e410, completion:7731315, storage:e0e7bed,
  Detent:abb78f7, consensus:d1065e0, prospect:eef601d, Xapiand:df35965 (on master).
- CI "nightly (failure)" at a2c60c7 is a docs-only commit's nightly. I did not use it.

## What I checked myself

- Probe job `261116-143957-91a3bb` (light, exit 1 = finding):
  `.scratch/review-faultlab-design-design/sizes.sh` builds `sizes.cc` against faultfs.hh,
  memory_io.h and io.h at the cited commits. It writes one unsynced 1536-byte append to a durable
  empty file and crashes it with 4096 seeds. Result: faultfs sizes `0 1536`, MemoryIO sizes
  `0 512 1024 1536`. Rerun it on a fix; it should exit 0 once faultlab reaches MemoryIO's states.
- Code reads, with the citations given in the findings: faultfs.hh crash() and io; memory_io.h;
  crashio.h and test_crash.cc reconstruct; test_relay_mirror_crash.cc and
  test_agent_store_crash.cc; mirror.cc and agent_store.cc; MemoryIO users in Detent and
  consensus; storage_io.h and the storage.h sync and fallocate callers; posix.h sync and options;
  the strata faultfs users and map switch; strata's parked simulator design D5.

## Answers to the lead's questions

1. **Does faultfs cover every state the other three models produce?** No. MemoryIO and CrashIO
   both leave a file whose size lies between the durable size and the current one, at a sector
   boundary. faultfs leaves only the old size or the new one (D1). Detent's `Entries::All` is
   the union of the durable and live namespaces, not "every namespace change kept" (D2).
2. **Does F4 cover MemoryIO's prefix use without changing what those tests prove?** Yes for
   consensus, whose `log/` and `g<N>/` prefixes are siblings (shared_cluster_test.cc:101,105;
   shared_worker_test.cc:105). Detent nests `history/` under a root IO on the same disk
   (test_namespace_power_cut.cc:125-126). F4 does not say whether a directory is an entry of its
   parent's scan (N2).
3. **Is F8's growing `preallocate` right?** Yes. Both storage callers grow the file
   (storage.h:547, 1438). In faultfs, the region past the durable size reads as zeros whether
   the sector is old or new, so only the size coin varies. That matches an unwritten extent after
   a real power cut. The error contract misses the unsupported case that storage tolerates (N3).
4. **Does an IO-level recorder see everything Detent's sync-hook recorder sees?** Not today.
   The flush stages are not the loss: an fsync staged and a later device flush become sync and
   ordering epochs, and faultfs keeps a superset of those states. The loss is every operation
   Detent's stores make outside the seam (D3).
5. **Does each migration step leave every repository green, and is the order right?** Step 7
   cannot go green as planned (D3). Steps 5 and 6 are safer swapped (N5). Under pinning, every
   other step holds.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | yes | defect | F3's claim that faultfs is "a superset of storage's CrashIO and journal's MemoryIO in fault space" is false. Over a durable 0-byte file with a 1536-byte unsynced append, MemoryIO's crash keeps any subset of pending sectors and sets the size to the end of the furthest kept one (memory_io.h:73-79). CrashIO's torn write applies the first 512 bytes and sizes the image to them (test_crash.cc:90, 94-96). faultfs picks only the old size or the current one (faultfs.hh:284). Deleting memory_io.h (step 8) and crashio.h (step 9) therefore loses the state "the file ends partway through an append, with no zero tail". Real filesystems reach it: ext4 and XFS advance the on-disk size as writeback completes. That state drives a parser's short-read-at-EOF path, which differs from its zero-tail or checksum path. | Probe `261116-143957-91a3bb`: faultfs `0 1536`, MemoryIO `0 512 1024 1536`. | Give `crash_policy` a size pick that cuts at any sector boundary between the old and current sizes, opt-in, so F3's seeds and their table stay unchanged. Alternatively the owner rules that the default changes and F3's table is re-recorded. faultlab's suite adds the probe as a test: every MemoryIO and CrashIO state over random append sequences is a faultlab crash state. F3's text states the result. |
| D2 | yes | defect | F5 maps `Entries::All` to `names = newest`, but Detent builds it as the durable namespace overlaid with every live name (test_relay_mirror_crash.cc:129-133; test_agent_store_crash.cc:145-149). The union keeps names removed since the last directory sync next to every new name. That is a legal state, and the collector's case: new table present, old files not yet removed. `names = newest` applies the removes, so it drops that state. The union also keeps both names of a rename, which atomic `replace` forbids and faultfs cannot produce. QD2's option (a), "those four only, deterministic as today", is therefore not as today. Detent's lead would rule on a false premise. | Code read, cited at left. faultfs replays each logged op under its own coin (faultfs.hh:303-307), so only seeded sweeps reach "creates kept, removes dropped". | Either add a `names` pick for "creates and renames kept, removes dropped" and map `Entries::All` to it, or state in F5 and QD2 that `names = newest` narrows Detent's sweep. In the second case, say which seeded sweep (QD2 b) restores the legal half, and that the both-names state is dropped as illegal. faultlab's suite then checks the chosen picks against Detent's `after_power_cut` outputs on one recorded run. |
| D3 | yes | gap | F10 says "every call goes to the real IO and then to the shadow". Detent's two stores do not go through `journal::IO` alone. Both take `std::shared_ptr<c::PosixIO>` (mirror.cc:30, 151; agent_store.cc:70, 251), so no decorator can be passed in. Both publish with raw `::linkat` on `native_directory_handle()` (mirror.cc:45, 249; agent_store.cc:84, 382), a namespace change the seam has no primitive for and faultfs has no op for. Both `::fstat` the directory (mirror.cc:120; agent_store.cc:201). The agent store opens `change` through `detail::ChangeWriter` on the directory fd (agent_store.cc:208). They also call `inspect_entry` and `acquire_owner(OwnerLockOptions)`, which faultfs does not implement: io.h:227 and 255-257 throw. Detent's recorder sees the links because it rescans the directory by inode (test_relay_mirror_crash.cc:45-58). The IO-level recorder would miss them, and its shadow would diverge from the real directory at the first link. QD1 option (a), recovery on faultfs, cannot run these stores at all. Step 7 lists no prerequisite for any of this. | Code read, cited at left. | Decide the link mechanism. One option is a third seam addition, an exclusive `IO::link(source, destination)` (linkat), with a faultfs op replayed in `crash` like create. That changes A2's "two additions" and is the lead's or the owner's call. The other is for Detent's stores to publish with `replace`. Then F7 adds faultfs's `inspect_entry` and `lock_owner`, and step 7 lists Detent's stores moving from `PosixIO` to `journal::IO` (or the recorder exposing what they need) as its first item, with the completion change in step 0. faultlab's recorder tests include a link. |
| D4 | yes | defect | F9's default contradicts itself, and the owner is asked to rule on that text. It says `sync` and `sync_data` "both are durable". It then says a process that opts for ordered syncs gets F_BARRIERFSYNC for `sync` and that faultfs's `ordered_syncs` makes `File::sync` "an ordering point rather than durable". Under that setting two things break. First, every journal, strata and Detent caller in the process loses `File::sync` durability; journal's recovery truncate-then-sync (journal.h:363-364) is one example. Second, the storage mapping inverts strengths: `full_fsync` maps to `sync` and gets the barrier, while the cheap path, `fsync` mapped to `sync_data`, gets F_FULLFSYNC (posix.h:184-191). | Code read: posix.h:173-191, storage.h:1211-1228, storage_io.h:208-229. | Restate option (a) so that `sync` and `sync_data` stay durable in every process. Then name which call storage's cheap path maps to. One candidate is `order_files_and_directory({file})`, which the seam and faultfs already model; note that it is a directory fsync on Linux. Or present the cheap path honestly as option (b) or (c). |
| N1 | no | gap | The F5 check "every `crash(seed, policy)` state equals some `crash(seed')` state" cannot be rerun as written: it means searching 2^64 seeds. | Migration-order "Checks" paragraph. | State it as "equals `crash` with each fixed field's coin forced", which a test can check directly. |
| N2 | no | gap | F4 leaves nesting unspecified. Detent opens a root IO and a `history/` IO on one disk (test_namespace_power_cut.cc:125-126). MemoryIO hides `history/` from the root's scan (memory_io.h:168-169); PosixIO would list it. `make_directory` "durable at once" also drops the state "directory missing after a crash" that a real mkdir without a parent fsync leaves. MemoryIO lacks that state too, so nothing is lost. | memory_io.h:121-172. | One sentence: whether a directory is an entry of its parent's scan, `entry_footprint` and `remove`, and that `make_directory` is durable by fiat. |
| N3 | no | gap | F8 names only ENOSPC and EIO. Storage tolerates EOPNOTSUPP, ENOSYS and EINVAL from fallocate and continues (storage.h:1438-1444); FreeBSD ZFS's posix_fallocate returns EINVAL, for example. F8's rejection of a no-op default is overstated: storage.h:547 only warns on failure. | storage.h:547-549, 1438-1447. | Say whether PosixFile falls back to the `truncate` growth on an unsupported filesystem or throws `errc::not_supported`, which storage keeps tolerating. |
| N4 | no | gap | strata's parked simulator design D5 specifies the same disk differently: a sparse sector store, tearing at 512 B and 4 KiB, hierarchical directories, and no `getenv`, because the map switch becomes a constructor option. It also measures faultfs's whole-file sync at 11 ms on a 64 MiB file. faultlab keeps the dense `std::string` store and `FAULTLAB_MAP` from the environment (F2), adds a deep `snapshot()` per recorder event, and adds `preallocate`, which lets journal's 64 MiB turn-log segments land on that store at step 6. | strata HEAD docs/design/simulator.md:274-300, 191-192. | Cite D5 and say whether faultlab's store becomes D5's or D5 builds on faultlab. Make the map switch a constructor option rather than renaming the environment variable. |
| N5 | no | nit | Steps 5 and 6: consensus moves journal's turn log onto faultfs before journal's own tests run on it. faultfs's namespace subsets, and its non-durable truncate (MemoryIO's `truncate` syncs, memory_io.h:105), can surface journal bugs, and those would turn consensus red first. | memory_io.h:105; consensus test/shared_*_test.cc. | Move journal's tests first, as the quest lists, and keep the memory_io.h deletion last. |
| N6 | no | gap | With `PowerLoss` replaced by EIO, the code under test runs its error paths after the cut. consensus core.h:256 swallows `std::exception` and fences, and retry loops get EIO on every mutation. The tests that catch `PowerLoss` (shared_cluster_test.cc:191, shared_worker_test.cc:221, 248) need a `powered_off()` check per step and a bound on retry loops. Steps 5 to 7 are sized as mechanical. | consensus include/consensus/core.h:256. | Say so in steps 5 to 7, or offer an opt-in to throw a non-`std::exception` type at power-off, as MemoryIO does. |
| N7 | no | nit | The recorder's shadow plays a group sync as its primitives in list order, so it reaches only prefix subsets of the group. io.h:232-243 leaves any subset possible. Detent's recorder sees one real order too, so nothing is lost; the gap is faultfs's default `sync_files`. | io.h:234-236. | Optional: a crash inside a group sync keeps any subset. |
| N8 | no | nit | F4 rejects one filesystem per directory, but step 3 keeps strata's 8 two-filesystem tests as they are ("include and namespace"). | faultlab.md F4, step 3. | Say whether they move to one device later, as their own change. Moving them changes their seeds. |
| N9 | no | nit | F10 leaves PosixIO's syscall mapping to completion's sync_calls_test. That mapping depends on `PosixIOOptions::full_flush_ordering` (posix.h:599-602). | posix.h:596-609. | sync_calls_test covers the options Detent runs with. |

## Not run

- No build of the doc's proposed API. The design has no code, and the probe is the only compile.
- No check of D1's state against journal's or storage's recovery code. That a lost state is
  never needed is unproven, which is why D1 is blocking.
- No check that Detent's recovery tolerates the both-names state, or depends on it.
- The F12 and F13 changes to history.py, and netlab and disklab (F14, F15), beyond reading them
  against the survey and history.py's line citations.
- No check of the sizes in the migration table beyond the MemoryIO user counts: journal has 4
  tests and 3 fuzz files, consensus 3 tests, Detent 4 authority tests plus the cluster harness.

## Re-check

Commit: cpp-libraries:bcf85b4b367352284d7bb0ed4c6e72b7da6be9f7
Verdict: APPROVE

This round covers the diff a2c60c7..bcf85b4. Line numbers are faultlab.md at bcf85b4. All four
blockers are closed. I re-checked D1-D4 only. I did not rerun the probe, because D1's text makes
the shape of its test clear.

### Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | no | defect | Closed. F3 withdraws the superset claim and cites the probe. An opt-in `size_pick::sector` reaches the partial-append sizes. The default coin stream, and so F3's seeds, stay unchanged. Steps 5, 6 and 9 sweep with the new pick, and the probe becomes a faultlab suite test. | faultlab.md:42-50, :76, :87-88, :318-319, :322, :334-336 | none |
| D2 | no | defect | Closed. `name_pick::additions` keeps creates, links and replaces and drops removes. Entries::All maps to it. The both-names state is dropped as illegal. A job lists every state the four picks no longer reproduce. | faultlab.md:75, :86, :90-99, :354-356 | none |
| D3 | no | gap | Closed. `IO::link` becomes a third seam addition. faultfs adds `link`, `inspect_entry` and `lock_owner`. Before Detent's crash tests move, its stores: take `journal::IO`; publish with `link`; check ownership through `directory_trust()`; open ChangeWriter's file through the IO. Step 7 does that first. `verify()` fails loudly on any change made outside the seam. | faultlab.md:125, :127, :134, :136-163, :187-188, :200-211, :320 | none |
| D4 | no | defect | Closed as text. `sync` and `sync_data` are durable in every process, and there is no barrier-only mode. One switch weakens all of a process's syncs together, and storage maps both of its syncs to durable calls. The owner still rules on F9. If a deployment sets the switch in place of STORAGE_FULL_SYNC, journal's and strata's syncs in that process weaken too. The text states this. | faultlab.md:165-176 | Owner's ruling |
| N4 | no | gap | Still open. The doc does not cite strata's parked simulator D5. The map switch is still read from the environment (`FAULTLAB_MAP`), and the store is still dense. | faultlab.md:36, :316 | As in round 1 |
| N7 | no | nit | Still open. The shadow plays a group sync in list order, so it reaches only prefix subsets of the group. | faultlab.md:197-198 | As in round 1 |
| N8 | no | nit | Still open. Step 3 keeps strata's 8 two-filesystem tests and does not say whether they move to one device later. | faultlab.md:65-66, :316 | As in round 1 |
| N9 | no | nit | Partly closed. sync_calls_test is cited per primitive and platform, but the `PosixIOOptions` that Detent runs with are not named. | faultlab.md:214, :346 | As in round 1 |

Folded in but not re-checked: N1 (:332-333), N2 (:58-62), N3 (:152-156), N5 (:325-327), N6 (:112-114, :318-319).
