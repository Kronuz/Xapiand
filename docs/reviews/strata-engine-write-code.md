# Code review: phase7a/strata-engine-write (S1, write slice)

Reviewer: code-reviewer, 2026-10-27. Branch head Xapiand:d29bbb7c7, diff `8170a3ef6...HEAD` (src/, tests/, harness/).
Read: ledger `docs/status/strata-engine-write.md`, `docs/mechanisms/strata-engine.md`, `src/engine/strata/{shard,document}.{h,cc}`,
the seam call sites in `src/database/{shard,wal}.cc`, and the pinned strata/prospect headers (`index.hh`) in the build's `_deps`.
`docs/reviews/calibration.md` does not exist in this tree. No Xapian or other GPL source was read.

## Verdict: APPROVE WITH CONDITIONS

No finding is reachable by an acknowledged write in a normal single-node, local, WAL-on flow, and the lead's five risk areas
mostly hold. Four conditions (CR1 to CR4) must be fixed in the one fix round before merge; each is small. The rest are notes.
The proof round is real: baseline suites reproduce (below) and my probes found no race, no leak, no lost write.

Baseline reproduced here (RelWithDebInfo, ASSERTS=ON, CRASH_POINTS=ON, LTO off, Homebrew LLVM 23):

| Build | `test_engine_strata` | `test_engine_replay` | Reports |
|---|---|---|---|
| TSan (own `_deps`) | 9482 checks pass | not run | 0 TSan |
| ASan+UBSan (own `_deps`) | 9482 checks pass | 17 pass | 0 ASan, 0 UBSan |

## Blocking items (conditions to fix before merge)

### CR1. `cancel_transaction` leaves a destroyed index behind when the reopen fails (heap-use-after-free)

`src/engine/strata/shard.cc:738` (`Shard::cancel_transaction`): `core.index.reset(); core.index.emplace(prospect::index::open(...))`.
If `open` throws (EMFILE, EIO, EACCES), `core.index` is left disengaged but the shared `Core` lives on, and every handle's next call
dereferences it. Probe `.scratch/s1-review/probe_errors.cc` (`cancel_reopen_fails`, ASan): after the failed cancel `stats()` returns
`doc_count=0` (garbage, no error), then `find_key` aborts with `heap-use-after-free` in `hash_slots::index_of`, freed by
`~basic_index()` inside `cancel_transaction`. The ledger's invariant 3 and "error paths" are not met here.
Reachability today: only through `Shard::cancel_transaction`, which the node calls from the remote and replication protocol
views, so not on a local strata shard yet; it becomes live with S4. Fix: open into a temporary and `emplace` only on success (on
failure keep the old index and surface the error), or mark the Core unusable and throw from every entry point. Add a test that makes
the reopen fail (chmod 0 on the strata dir) and then calls a read.

### CR2. Dropping the last handle in the middle of a transaction commits the transaction

`Core::~Core` (`shard.cc:180`) commits `if (index && pending)` and ignores `transaction`. Xapian rolls back (the node's `do_close`
deliberately skips the commit when `is_transactional()`, then drops the engine shard and re-begins the transaction on reopen,
`database/shard.cc:458`). Probe `probe_errors.cc` `drop_in_transaction`: commit one document, `begin_transaction(true)`, add a
second, drop the handle, reopen: Xapian reports `doc_count=1`, strata `doc_count=2`. A later `cancel_transaction` then returns to a
state that already contains the half transaction. Done criterion 1 says "the last handle commits, as a Xapian writer does", and it
does not when a transaction is open. Fix: no commit in `~Core` while `transaction` is set (the pending writes are dropped with the
Core, and the WAL replays them into the re-begun transaction, as it does for Xapian). Add the `drop_in_transaction` check to the
shared test so both engines are held to one answer.

### CR3. A fenced index is never replaced while any handle to it lives

Probe `probe_errors.cc` `fenced` (writable and readable handles open; the strata dir made read-only, then fixed): commit fails with
`database: ... create journal file: Permission denied`, after which every write and commit on the writable handle, and on a
*new* writable handle opened while the readable one lives, fails with `the writer is fenced; reopen the index`.
`Shard::reopen()` returns `false` ("a handle always sees the latest", `shard.cc:806`), so the node's `retry_or_throw` ->
`do_close` -> `reopen_writable` loop re-acquires the same fenced `Core` (the readable handle in the pool keeps it open). Only
closing every handle recovers it (here the later `add` worked, and the pending `Qb` was gone, as it must be; the WAL, absent at engine
level, would restore it on the node). A transient I/O error therefore wedges the shard until the pool evicts the readable
handle, where Xapian would reopen from disk. Fix: `reopen()` (or `acquire`) rebuilds a Core whose index is fenced (drop the
index, reopen from the last commit), then the writable open's WAL replay recovers the pending writes. Test with the probe.

### CR4. "Replay is idempotent per line" is false for a log with an erase followed by a re-add of the same key

Probe `probe_replay.cc` (`double_replay`, TSan and ASan): replaying a log twice over the state it produced.
`noerase` (put, put, put) and `erase_only` (put, put, erase) are idempotent. `erase_readd` (put a, erase 1, put a), `readd_replace`,
and `two_keys_swap` fail on every pass: the re-created key now lives at a new docid, so the replayed first line raises
`invalid: The key belongs to another document` (`shard.cc` `replace`) and the replayed erase raises `doc_not_found`; the state is
unchanged (loud, not corrupt), but a replay that throws aborts `DatabaseWAL::execute` and the shard does not open. The ledger
(item 7 under Done, and `strata-engine.md` "Replay is idempotent per line") claims the stamp rule covers this and explains the
erase gap by the revision check. The revision check is what actually protects the node: a replay starts from the committed
revision, so double application needs a Core that kept uncommitted writes across a writable reopen. I could not make the node
do that except after a failed close-commit (`do_close` commits first, `Shard::reopen` returns early for strata), so this is
not reachable in a normal flow. Fix (either): state the contract precisely in the note and ledger (idempotent for logs without an
erase of a key that is later re-added; otherwise the revision check is the only guard) and add the three failing logs to
`replay` as expected-failure checks; or stamp erases and re-point `replace` at the key's current docid on replay. Related hardening, same
fix round: `replace`'s skip rule is `stored >= stamp`, so a line stamped v3 over an index at v1 applies as v2 and the version
drifts silently (probe `stamp_gap`: replica v2, primary acknowledged v3). Apply only when `stamp == stored + 1` (or `1` for an
absent key), skip when `stored >= stamp`, else raise `errc::corrupt`.

## Answers to the five risk areas

1. **Replay ordering.** Fresh replay of a log with erases, an erase then re-add of the same key (new docid, version back to 1),
   and a replay split at a commit boundary (reopen from disk between the halves) converges to the primary's exact state
   (`probe_replay.cc` `erase_readd_fresh`, `commit_boundary`: docids, versions, data, `lastdocid` equal). A replayed erase of an
   absent document is `doc_not_found`, safe only behind the revision check. Double replay: see CR4.
2. **Version stamp in the codec.** The stamp was added under the same format byte (`serial_version = 1`, `document.cc:~30`,
   layout change in 9cbd16f51) with no bump. Probe `probe_oldline.cc`: 20000 randomised pre-stamp lines all fail as
   `errc::syntax` (none decoded wrongly), so an old log aborts replay loudly rather than misparsing; no such log exists outside
   dev trees because strata has not landed. Cheap to close: bump the byte to 2 so the error says "unknown version". A stamp cannot
   disagree with the version map across a crash on a faithful replay: the stamp is `stored + 1` taken before the apply, the
   `_version` in the stored data is rewritten by `finalise_write` with the same number, and pending writes die with the process,
   so replay recomputes the same versions. It can disagree only if a middle line is missing (CR4, hardening).
3. **Concurrency beyond `core.mtx`.** Every public entry point of `Shard` takes `core.mtx` (checked line by line), no lock is held across a
   `co_await`, and `uuid()` reads an immutable member. Probe `probe_conc.cc` under TSan (0 reports) and ASan (0 reports): four reader
   threads (`get`, `find_key`, `scan`, `stats`, `doc_count`, `revision`, `metadata`) on the writable and a second readable handle during
   1200 writes with a committer thread (a refresh per read) and a thread opening and closing handles, once inline and once
   through a 4-thread pool scheduler (the offload hop); then 300 rounds of "last handle closes and commits" racing three
   reopeners. Invariants (no backwards version, scan order, count at least the acknowledged count) hold. Close during a write by
   another handle is safe by construction (`Entry::mtx` serialises the last release against `acquire`); destroying the only
   handle under an in-flight coroutine is a contract violation, as in the Xapian shard. Notes: the last release commits (fsync,
   merges) on whichever thread drops the shard, including a loop thread if one does; `scan` materialises every live document's
   stored data in a `std::vector<Hit>` under the shard lock (`shard.cc` `scan`), so `dump_documents` on a large shard needs the
   whole shard's stored data in memory and blocks writers meanwhile (read from the code, not measured).
4. **Crash points.** Yes, `CRASH_POINTS` changes the committed path: `Core::commit` calls `index->refresh()` before `index->commit()`
   only under `#ifdef XAPIAND_CRASH_POINTS` (`shard.cc:198`). Production does flush and commit inside one `commit()`; the
   crash build adds a second `maybe_merge` pass and a window ("the segment is on disk and the manifest does not name it")
   that the production call also has, but inside strata, where the harness cannot stop it. The extra refresh is cheap and
   almost certainly equivalent (`run_merges` loops until the policy asks for none), so the fix is to call `refresh()` before
   `commit()` unconditionally, which makes the tested path the shipped one (CR5, non-blocking). A kill between the WAL append and
   the strata commit is covered by `commit.before` (the WAL line is appended at write time, the commit is 3 s or more later;
   a kill -9 keeps page-cache writes, so this is a process-kill test, not a power-loss test), `commit.flushed`, `commit.after`,
   and mid-replay. A kill between engine apply and the asynchronous WAL append loses an unacknowledged-in-WAL write exactly as
   Xapian does. I did not rerun the node scenarios (see Not run).
5. **0xff key adapter.** Confined to `src/engine/strata/shard.cc`: the constant `key_lead`, `engine_key()` (6 call sites in that
   file) and `index_options().key_prefix`. Nothing else in `src/`, `tests/`, `harness/` or the docs names the byte; `Document::set_key`
   (the seam) is engine-neutral. Replacing it by the fields-and-keys design (strata `design/fields-and-keys`, "no key prefix, keys leave
   the manifest") is a change to those three places plus a format migration (the index stores the prefix in its manifest; formats
   are open until release). It is not gated by a named seam like `values.h`; moving the three places into `src/engine/strata/keys.h`
   would make that explicit (CR6, non-blocking). The `Q` prefix of Xapiand id terms and field terms leading with `Q` are unaffected.

## Non-blocking notes

- CR5. Always call `index->refresh()` before `index->commit()` (see risk 4), so the crash build and the shipped build run one path.
  Also: the windows inside `commit_files` (live files written, manifest renamed, directory synced) are only reached by strata's own tests.
- CR6. Put the key adapter behind `src/engine/strata/keys.h`.
- CR7. `~Core` swallows a failed commit (`catch (...) { }`) with no log line, and the failed index may then be fenced; log it.
- CR8. `Shard::apply` throws `errc::internal` after the index has already applied the operation if the verdict's docid or version differs from
  the expected one (`shard.cc` `apply`), leaving a write with no WAL line; unreachable under the shard mutex but it contradicts
  invariant 3 ("a rejected write leaves no trace"). Make it an assert, or undo.
- CR9. `erase` by docid after any write forces a flush (`locate` -> `refresh`), so an erase-heavy mix creates many small segments (ledger
  finding 2 already asks strata for `find_docid`).
- CR10. Strata shards have no remote or replica story: `require_local` refuses remote writes, `begin_transaction`/`cancel_transaction`
  carry the CR1 and CR2 defects into the replication protocol views when S4 arrives. Architect: confirm the engine choice per index
  is refused or documented for clustered indexes until then.

## Probes and artefacts (all in `.scratch/s1-review/` of the worktree)

- `probe_replay.cc`: replay ordering, erase and re-add, double replay (6 scripts), stamp gap, commit boundary. TSan and ASan. 7 intended failures: the CR4 cases and `stamp_gap`.
- `probe_oldline.cc`: 20000 pre-stamp lines through the codec.
- `probe_conc.cc`: readers during writes, handle churn, last close vs reopen, inline and pool scheduler.
- `probe_errors.cc`: drop mid-transaction (both engines), fenced index, failed `cancel_transaction` reopen.
- `mkprobe.sh tsan|asan FILE.cc`: builds a probe against the branch's objects using the test target's own flags (needs `build-s1-<san>`).
- `build.sh`, `cfg-*.log`, `build-*.log`, `run-tsan/`, `run-asan/` (outputs), `cmds-tsan.txt`.
- Build recipe that worked (Homebrew LLVM 23 at `/usr/local/opt/llvm`): configure with `-DASSERTS=ON -DLTO=OFF -DCRASH_POINTS=ON
  -DCMAKE_CXX_FLAGS="-fsanitize=<san> -isystem .scratch/sysinc"` and `-DUUID_INCLUDE_DIR=.scratch/sysinc -DZLIB_INCLUDE_DIR=.scratch/sysinc`
  (otherwise the SDK `usr/include` lands as `-isystem` and breaks libc++), `SDKROOT=<CLT SDK>` in the ninja environment, and rapidjson
  `GenericStringRef::operator=` made `= delete` in the build's own `_deps`.

## Re-check

See "Re-check 1" at the end of this file.

Not run: the node scenarios `harness/strata_write_check.sh` (no `xapiand` binary was built; the proof round's numbers are taken from the
ledger), the other 19 ctest suites and the Xapian-only checks, the libFuzzer target, power-loss or fsync-ordering durability, Linux and
FreeBSD, CI, benchmarks, scan memory under a large shard (read from code), a security review (no untrusted-input surface beyond the WAL codec,
whose decoder returns `errc::syntax` on 20000 old-layout lines and the existing fuzz target).

## Re-check 1

Re-checker: rechecker, 2026-10-28. Fix round Xapiand:d0a664b8c (code) and Xapiand:d6c2eaa3e (ledger) on `phase7a/strata-engine-write`,
local only. Read: the two commits' diff and the callers they touch (`src/database/shard.cc` `retry_or_throw`/`reopen`, `src/database/wal.cc`
`execute_line`). No GPL source read. Reused the reviewer's probes unchanged (`.scratch/s1-review/probe_*.cc`, identical to `.scratch/s1-fix/`).
New probes and scripts are in `.scratch/s1-recheck/`: `probe_wal.cc` (CR4 through `wal.cc`), `probe_fence2.cc`, `probe_fence3.cc` (NF1),
`mkprobe2.sh` (builds a probe with the link line of `xapiand_test_pool`, which links `wal.cc`), `mutants.py`, `cfg.sh`, `build.sh`.

Builds (own dirs, own `_deps`, Homebrew LLVM 23, RelWithDebInfo, `-DASSERTS=ON -DCRASH_POINTS=ON -DLTO=OFF`):

| Build | `test_engine_strata` | `test_engine_replay` | Probes | Reports |
|---|---|---|---|---|
| ASan+UBSan | 9549 pass | 17 pass | conc 806 pass, oldline 20001 pass, replay and errors as below | 0 |
| TSan | 9549 pass | 17 pass | conc 806 pass, replay and errors as in ASan | 0 |
| ASan+UBSan, other suites | `engine_shard` 54, `engine_document` 645, `engine_census` (off) pass | | | 0 |

### Verdicts

| Item | Verdict | Evidence |
|---|---|---|
| CR1 | CLOSED | `shard.cc:195` `Core::check()` is called first under `core.mtx` by every entry point (checked: `add`, `replace`, `replace_by_key`, `erase`, `erase_by_key`, `commit`, `begin/commit/cancel_transaction`, `set_metadata`, `stats`, `find_key`, `get`, `scan`, `metadata`, `metadata_keys`, `revision`, `doc_count`, free `check()`); `reopen_index()` (`shard.cc:204`) is what `cancel_transaction` (`:816`) calls. `probe_errors.cc` under ASan and TSan: `cancel_reopen_fails` gives `database: open journal scan cursor: Permission denied`, then `stats()` gives `database: The strata index is closed after a failed reopen`, with no ASan or TSan report; the probe then ends at its own unguarded `find_key` (an uncaught `engine::Error`, the probe's last line, not a crash in the code). Test `cancel_reopen_fails` covers `stats`, `find_key`, `get`, `add`, `commit`, `metadata`, `doc_count`, `revision` and the recovery by `reopen()`. Mutant (`check()` disabled): ASan heap-use-after-free at the test's `stats()` call, the original finding. |
| CR2 | CLOSED | `shard.cc:181` `~Core` commits only when `!transaction`. `probe_errors.cc` `drop_in_transaction`: xapian `doc_count=1`, strata `doc_count=1` (was 2). Test `drop_mid_transaction` runs both engines and then a cancel. Mutant (`!transaction` removed): 3 checks fail (`test_engine_strata.cc:721, 722, 726`). |
| CR3 | CLOSED for the finding as written; see NF1, which this fix introduces | `shard.cc:96-116` `rethrow_translated(core)` marks the Core on `index_fenced`; `acquire` (`:327`) and `Shard::reopen()` (`:872`) replace a fenced or empty index with one opened from the last commit. `probe_errors.cc` `fenced` (ASan, TSan): commit fails with `durability failure`, then the write fails as fenced, `reopen(): 1`, a new writable handle opened while the readable one lives adds `ok`, and after every handle closes `doc_count=3` (was a wedge). `probe_fence2.cc` (no intermediate write): the first write on a new writable handle after the failed commit is `ok`, so the first failure marks the Core by itself (it is an `index_fenced`). Tests `fenced_recovers(true/false)`. Mutant (`unusable()` always false): the reopen and new-handle tests fail. |
| CR4 | CLOSED | Stamp rule: `replay_applies` (`shard.cc:443`) skips when `stored >= stamp`, applies only as `stored + 1` (1 for an absent key), else `errc::corrupt`; used by `replace` (`:597`) and `replace_by_key` (`:646`). Moved key: `replace` skips a replayed line whose key lives at another id (`:578`). `probe_replay.cc` (ASan, TSan): `erase_readd`, `readd_replace`, `two_keys_swap` now end with state equal to the primary's and the only error left is the engine's `doc_not_found` for the replayed erase; `stamp_gap` gives `corrupt: The log skips a version of a document` and the replica stays at v1 (its `v == 3` check still reports, as intended: the probe asserted the old drift). Mutants: moved-key skip off (1 check fails), gap rule off (4 checks fail). **`wal.cc` itself** (`:563-570`): the ledger covered it only through an engine-side copy, so I wrote `.scratch/s1-recheck/probe_wal.cc`, which opens a shard through `DatabasePool`, builds lines as `DatabaseWALWriterTask` does and runs `DatabaseWAL::execute_line`. With the fix: an erase of a missing document returns `ok` on both engines (strata and xapian), and a double replay of `put a, erase, put a` through `execute_line` is `all ok` twice with the primary's state (5 checks). With the `wal.cc` hunk reverted: strata fails 3 of 5 (`doc_not_found` from the erase, twice in the replay), xapian fails 1 of 1; restored afterwards. |
| CR5 | CLOSED | `shard.cc:218-226` `Core::commit` calls `index->refresh()` then `crash_point("commit.flushed")` then `index->commit()` with no `#ifdef`; `crash_point` is the no-op inline outside `XAPIAND_CRASH_POINTS`, so the crash build runs the shipped path. Not observable through the API (as the ledger says). |
| Format byte 2 | CLOSED | `document.cc:36` `serial_version = 2`; `:242` a format 1 line raises `errc::syntax` "format 1 predates the version stamp and is no longer read"; any other byte is "unknown version". `probe_oldline.cc`: 20000 old-layout lines, 0 decoded, 20000 `syntax` (ASan and TSan not needed for the codec; run under ASan). Tests `old_format` and `codec`; mutant (byte back to 1) fails the codec, replay and old-format tests. |
| CR6, CR7 | CLOSED (non-blocking) | `src/engine/strata/keys.h` holds `lead`, `engine_key`, `key_prefix`; `~Core` logs a failed commit with `L_ERR` (`shard.cc:181-191`). |
| CR8 to CR10 | open as recorded in the ledger | not in scope of this fix round. |

### Node scenarios

`xapiand` built (Homebrew LLVM, `-O2 -g`, `-DASSERTS=ON -DCRASH_POINTS=ON -DLTO=OFF`, own dir) and
`BIN=... PORT=8931 harness/strata_write_check.sh write replay versions commit replaycrash` run to the end under
`quiet light` and `timeout 3000`: 36 checks `ok`, 0 `FAIL`, final `PASS`; all five scenarios, including the crash points build
(`commit.before`, `commit.flushed`, `commit.after` each fired and recovered with 40 of 40 documents verified, twice;
the two mid-replay crashes fired and the third restart converged). `ps` afterwards: no `xapiand` left.

### New finding

**NF1 (blocking, introduced by the CR3 fix). A handle other than the writer that replaces a fenced index silently drops the live writer's
uncommitted writes and its transaction.** `acquire` (`shard.cc:327-333`) and `reopen()` call `reopen_index()` (`:204`), which sets
`pending = dirty = transaction = false`, drops the index and opens a fresh one, whichever handle asked and whatever handles are live.
The writer is told nothing: its next call passes `Core::check()` (the index is engaged) and works on the fresh index.
Reachable on a node because a *read* flushes the writer's buffered documents (`core.refresh()` in `get`, `stats`, `scan`, `doc_count`): a flush that
fails with an I/O error fences the shared index and raises `errc::database` on the reader; `read_fate` is `close_fail` for `errc::database`
(`engine/error.h:94-96`), so the node closes that reader; the next request opens a reader, `acquire` replaces the index, and the writer's acknowledged writes
(their WAL lines already carry the revision) are gone from the index. A later commit moves the revision on, so the WAL never replays them.
Before the fix the fenced Core stayed fenced, the writer's next write failed, the node closed the writer and the WAL replayed: no loss.
Probe `.scratch/s1-recheck/probe_fence3.cc` (writer adds `Qb` unflushed; directory made read-only; reader `stats()` fails with `durability failure`;
directory fixed; a second reader opens): `find_key("Qb")` through the writer is `0`, `add Qc` and `commit` succeed, and after reopening
`doc_count=2`, `Qb=0`, `Qc=2`. Reproduces under ASan.
Fix (small): give `Core` an epoch bumped by `reopen_index()`; a writable `Shard` records the epoch it has seen when it is opened (or when it
called `reopen()` itself) and `Core::check()` raises `errc::database` ("the index was replaced; reopen the shard") for a writable handle
whose epoch is behind, so the node closes the writer and the writable open replays the WAL. Add `probe_fence3` as a test (it fails now,
passes with the epoch). The ledger sentence "pending writes are dropped (the WAL replays them on a node)" is true only for the handle that
opens the writable shard afterwards, which is what the epoch makes the only path.

Side notes, not findings: `Shard::reopen()` on a fenced engine returns `true` without a WAL replay (`database/shard.cc:715`), which the
epoch above also covers; `probe_wal.cc` is worth keeping as `tests/test_wal_replay.cc` (it needs a local `Node` set and a relative path for the
`Endpoint`, both in the probe).

### May S1 land

No: CR1, CR2, CR4, CR5 and the format byte are closed and CR3 meets its own criterion, but NF1 turns a transient I/O error plus a second
handle into silent loss of acknowledged writes, which is the failure the WAL exists to prevent. The fix is the epoch above plus the
`probe_fence3` test; a re-check of NF1 alone is enough after that.

Not run: the other 15 ctest suites (only `engine_shard`, `engine_document`, `engine_census` and the two named ones), the libFuzzer target,
TSan or ASan on the `xapiand` binary and the node scenarios under a sanitizer, power-loss or fsync-ordering durability, Linux and FreeBSD,
CI, benchmarks, a node-level run of NF1 (reproduced at the engine API only, through the same calls the node makes), the mutant for CR5
(none can fail).

