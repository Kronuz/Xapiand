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

(none yet)

Not run: the node scenarios `harness/strata_write_check.sh` (no `xapiand` binary was built; the proof round's numbers are taken from the
ledger), the other 19 ctest suites and the Xapian-only checks, the libFuzzer target, power-loss or fsync-ordering durability, Linux and
FreeBSD, CI, benchmarks, scan memory under a large shard (read from code), a security review (no untrusted-input surface beyond the WAL codec,
whose decoder returns `errc::syntax` on 20000 old-layout lines and the existing fuzz target).
