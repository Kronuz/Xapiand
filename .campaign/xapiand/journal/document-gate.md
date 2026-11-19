# document-gate: journal

State: waiting on jobs 261118-202755-3d88cf 261118-202758-1425d5 261118-202802-fb1582 261118-202804-4d7ad8
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

2026-11-18: main (strata:aca39c6c, docs only) merged as strata:3ec3adbb; gate report gains the A6
VM rows, A7 checks and the VM wal1 miss (strata:69b1d4b9); build.sh builds RocksDB without
io_uring (strata:cbf08ebf, the lead's); calibrate runs trials and checks the pin (strata:a93f2b59,
pushed; CI 44440356088). Code under include/ unchanged since cc8ba3aa. build-test 261118-132434-8660f1
passed at 69b1d4b9.

A5: the first calibration at 100 B (261118-132449-745336) failed on my own exact-match check,
not an assertion: one trial picked lvl-4k-none, the pin scored 0.989 of it (fillrandom favors
lvl-16k-lz4, readrandom lvl-4k-none). 1 KiB 261118-132457-0e435f cancelled as superseded.
At a93f2b59: build-test 261118-175420-dca1f5 passed; CI 44440356088 green. Calibration, 5
trials: 100 B 261118-175418-fa1d4d passed (lvl-16k-lz4 fastest outright, next lvl-4k-lz4-hash
0.967); 1 KiB 261118-175419-c46cd4 failed: lvl-4k-none fastest, the pin 0.970 (fillrandom 343k
against 301k ops/s, spreads 3.8% and 8.3%; readrandom 0.94x, ycsb_a tie). A real miss, so 1 KiB
re-pins to lvl-4k-none. On the host only sync1's YCSB A and F use 1 KiB (wal1 and N-log are 100 B).
Queued: sync1 x3 at the new pin 261118-202755-3d88cf, -202758-1425d5, -202802-fb1582; a fresh
1 KiB calibration checking lvl-4k-none 261118-202804-4d7ad8.

## Next

1. Read the four jobs above; replace the sync1 rows (gate report, perf note) with the 1 KiB
   re-pinned ones, state the pins per size, then mark ready for review.
2. The lead's: the A/A sync rows and the VM syncaa 1 KiB row ran on lvl-16k-lz4 (A/A measures
   noise; VM rows check the platform); rerun them on the new pin or accept.
3. Review of the F1 fix at strata:db443563 (code cc8ba3aa); the lead's.
4. Packing follow-up once Detent's journal change lands: drop `turn_records` = 1, measure wal1.

## Blocked on

- (ruled by the lead: option (c); backpressure built, packing waits for Detent's journal change) D107 step 1 (F1), 2026-11-17: journal cc95ba6 (= journal main) cannot carry several records
  per sealer call. `Sealer` is `optional<TurnRecord>(room, first)`, one record at the group's
  staged + 1; `seal_turn` (turn_driver.h:451-506) calls each group's sealer once per turn, and
  `room` budgets that one record. Merging closed records into one strata record would move
  positions {s, j} already handed to `applied` (ruled: records stay whole). Packing needs a
  journal contract change, so I stopped; no code changed. Options:
  (a) Journal, with Detent: `seal_turn` calls a sealer again with the room left (first =
      false) until it returns nothing ("until nothing", not "once"); about ten lines plus a
      journal test. Then D107 as written: pack plus a byte-bounded backlog.
  (b) Strata only: apply waits while a closed record is unsealed (backlog = one command). The
      window is one barrier plus one write; memory is open + closed + in flight, three
      commands per index. Throughput caps at one record per turn: kv_strata's 1 MiB command at
      ~21 ms barriers is about 48 MiB/s, roughly 400k ops/s at 120 B against wal1's 924k.
      turn_sealer requires segment >= 2 x framed(command), so kv_strata can raise its command
      to half its segment (2 MiB at 4 MiB, 4 MiB at 8 MiB); packing's ceiling is the whole
      room, about two such records per turn.
  (c) (b) now, then (a) as a throughput change measured on the wal rows.
  Recommendation: (c). (b) gives D107's bound and bounded memory inside the file set today;
  what it costs on wal1 is measured and is exactly what (a) wins back.

- (ruled D105: option (b) on own VMs, arm64 by CI smoke now) A6 (platform rows): what is owed is every A3 row set on Linux x86_64, FreeBSD
  x86_64 and arm64: fits1 (13 rows), twox1 (13), big1 (10x: 3 rows, and the 10x rows A3 still
  owes on macOS), sync1 (6), wal1 (3), the N-log group-commit rows, and the A/A on each
  platform. None have run. Options:
  (a) CI carries them: a workflow_dispatch job running run.py's `dry`, then a few trials of
      fits1, sync1 and wal1 on GitHub's ubuntu x86_64, ubuntu-24.04-arm and the FreeBSD qemu
      runner. This is free and needs no VM. But shared runners and virtual disks make `sync`
      barriers and the absolute ratios unrepresentative, so they are functional rows with an
      A/A, not gate rows.
  (b) A dedicated quest VM from tools/vm/linux.sh and freebsd.sh, created in this worktree's
      .scratch on its own ports (for example 18782 and 18784), leaving strata's (18772, 18774)
      and Detent's (18762, 18763) alone. HVF qemu with cache=none on this Mac, runs under
      `quiet exclusive`. The disk image is 220G (Linux, sparse) and 40G (FreeBSD). Their
      `F_FULLFSYNC` and fsync barriers measure qemu on APFS, not a device, so the durable rows
      are labelled VM rows.
  (c) arm64 has no host beyond GitHub's runners (D36): option (a) or wait for one.
  No AWS (owner, 2026-11-04). Recommendation: (a) for arm64 and as a smoke test everywhere, and
  (b) for the Linux and FreeBSD none/wal rows. The durable tiers stay macOS-only until bare
  metal exists, stated in the gate report. This needs a decision: the gate's platform claim
  changes under any of these options.

- (none from the owner: Q1/Q2 ruled D104 by the lead, Q3-Q5 D103.)
- Journal (Detent reviews): `TurnDriver::seal`. The shim (strata `turn_seal.hh`) shows the
  contract needs four things beyond seal(group, fn): unseal by owner, a thread-safe wake for an
  open record's first transaction, the turn budget's room passed to the sealer, and a lock order
  (turn mutex, then record mutex). Doc "Built" lists them.

## Messages

- msg (lead, F1 option (c)): built the backpressure, the lag test and mutants M-s, M-t, N1-N4;
  probe and rows queued (ids under Next 0). Packing waits for Detent's journal change.

- Lead (2026-11-17, durability review running): A7 queued at 12f59669 (fuzz x4, version and
  fixtures, coverage); ASan/UBSan/TSan point at CI run 44119676721; scale statement in the gate
  report; A6 options under "Blocked on"; reviewed code untouched.

- Lead (2026-11-17, rows finished): read every row log; wrote docs/perf/standalone-turn-log.md
  (batch-1 ties, batched rows tie after the fix, wal1, A/A, the group-commit gap stated as
  turn-log-parity A6, the ack=none window) and docs/gates/document-profile.md; filled Evidence;
  main already merged (no new commits); pushed; fixed FreeBSD CI's served-test flake; ready
  for review.

- msg-1 (lead, journal-host-guards): repinned journal to cc95ba6 (landed). turn_sealer mirrors
  durable(): a wait on a group closed here, or at or below what its closed life retired, throws
  journal::group_closed; await_durable catches it by type and throws index_closed (header doc:
  after a leave the transaction is durable; after a close that could not leave, recovery tells).
  The restaged table record follows max(base, retired). Tests: t6w (a waiter blocked across
  close, then an old-life wait after reopen on the same host, then recovery over the gap at
  base + 2); build-test, TSan and T6 rerun as jobs (ids under Next).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | At strata:cc8ba3aa: build-test 261117-134744-7f0c17; TSan lag+served+T4+T12+T3+T8+t6w 261117-122105-d69a24 and batch TSan 261117-122114-58e309 (3d5ecedb, same code but t9o's test). T1 200-seed sweep 261117-033250-f42f82 (9d7c7710). Fails without: M-s 261117-122123-3b0f9a, M-t 261117-122132-b7e7f7 (lag); M-v 261117-180849-82b11b (t9o, rebuilt on cc8ba3aa as df87b29f; queued, its first run was lost; 261117-122140-f6bc36 does not count, t9o failed on the fix too); M-r 261117-055220-ecd5d0, M-d 261117-052940-986f82, M-p 261117-033229-e6ebfc, M-q 261117-033233-7e4d9b, M-o 261117-033236-b7fbb6, M-n 261117-033244-6cd288, M-b 261117-033248-e735a3, M-j 261117-030347-7f586d, M-k 261117-030357-c294a1, M-a 261117-030405-14df88, M-c 261117-030412-4cd813, M-h 261117-030420-64cdd2, M-e 261117-025024-24bfef, M-f 261117-025028-47bf98, M-i 261117-025035-d9bfa2, M-l 261117-025039-d9b68d, M-m 261117-025043-a4a6fc, and 261116-211626-0daa5f, 261116-211632-9ff248, 261117-025010-4c8220, 261117-025013-a6cec4, 261117-025017-0b218d, 261117-025020-93f7da, 261117-025032-8fc28b, 261117-025046-ec1d6e, 261117-025050-ee8915 |
| A2 | [artifact] strata `docs/gates/document-profile.md` (eb8437bc), "Prerequisites": format13 76fa3313, shared-reader-views 23e78bb6, write-buffer-reads 2f56d64c, manifest-log 15d39c30, all ancestors of cc8ba3aa |
| A3 | Durable tiers at cc8ba3aa, exclusive: wal1 261117-134809-8409da; sync1 261117-134818-6068ec, 261117-134824-199dc8, 261117-134834-f21397; N-log and group commit 261117-134842-7d024d; review probe 261117-134801-f716c7 (lag 2 records). A/A 261117-055320-5ca39d, 261117-055327-074989 (193e6e32). none tier fits/2x: main 75d1e7e (`core-vs-rocksdb-f14.md`). Owed: 10x rows, none tier at the gate commit |
| A4 | The same jobs; four axes per row in strata `docs/perf/standalone-turn-log.md` (db443563): sync ties (0.93-1.01x); wal1 1.68-1.94x faster, p99 7.8-11.9x lower, p99.99 113-179x higher; group commit tie at K=1 and K=8, 1.32-1.39x slower at K=4 (turn-log-parity A6); memory 2.5-3.4x, disk 3.3x on batched fills, 57-91% more written |
| A5 | Calibration at strata:a93f2b59, exclusive, 5 interleaved trials, a pin passing within 3% of the fastest: 100 B 261118-175418-fa1d4d (lvl-16k-lz4 fastest); 1 KiB re-pinned to lvl-4k-none after 261118-175419-c46cd4 (lvl-16k-lz4 at 0.970), checked by 261118-202804-4d7ad8 (candidates lvl-4k-lz4, lvl-4k-none, lvl-4k-zstd, lvl-4k-lz4-hash, uni-4k-lz4); every A3 row job runs RocksDB 11.11 pinned on it; harness per strata `docs/perf/rocksdb-fairness-audit.md` |
| A6 | D105, D114, at db443563 (code cc8ba3aa). Linux: build 261117-200358-f31d88, dry 261117-200358-e66125, sync+wal1 once 261117-200358-1336ac. FreeBSD: build 261117-200402-b01248, dry 261117-200402-6b0f87, sync+wal1 once 261117-200402-ed7288. Rows in strata `docs/gates/document-profile.md` "VM rows" (69b1d4b9). Failed first builds 261117-181304-9e575f (Linux), 261117-181307-6a39bf (FreeBSD): causes under Rulings; replaced by the two builds above. arm64: CI linux-arm64 smoke; rows owed before Phase 3R's next gate (D105) |
| A7 | At db443563: fuzz (3 workers x 25 min, ASan+UBSan) stored 261117-181138-259eee, manifest_log 261117-181138-24cc48, manifest_log_file 261117-181138-0354ba, command 261117-181139-098eed; version rejection and fixtures 261117-181139-9949e7; coverage 261117-181139-6e2a77; ASan/UBSan/TSan on CI. Scale statement: [artifact] gate report "Scale" (strata:69b1d4b9) |
| A8 | [artifact] strata `docs/gates/document-profile.md` (69b1d4b9): every measured row with commit, platform and jobs, the A7 checks, and the owed list |

## Result

strata `docs/design/standalone-turn-log.md` ("Built"); code in `include/strata/core/command.hh`,
`turn_seal.hh`, `index.hh` (logged mode), `test/standalone_turn_log_test.cc` (T2, T4, T9, T11,
T12, served; now T1, t1r, T3, T5, T7, T7b, T8, T10 too), `fuzz/fuzz_command.cc`, fixtures
`test/fixtures/command/`, `bench/kv/kv_strata.cc` (host). Mutant commits on local strata branches
`mutant/document-gate/m-*` (m-j 9ff37a0a, m-k 1f827405; m-a..m-o on 8855d678, applied by
`.scratch/mutate.py` in the strata worktree).

## Rulings I made

- A6 VM builds (2026-11-18, the first runs at db443563 failed):
  - Linux 261117-181304-9e575f: RocksDB's CMake found liburing (WITH_LIBURING defaults ON), so
    librocksdb.a calls io_uring, and `bench/engines/rocksdb-11.11/build.sh` links kv_rocksdb
    without -luring: `undefined reference to io_uring_queue_init`. build-linux.sh now removes
    liburing-dev before the RocksDB build (the macOS and FreeBSD builds have no io_uring).
    The lasting fix, -DWITH_LIBURING=OFF in build.sh, is strata:cbf08ebf.
  - FreeBSD 261117-181307-6a39bf: the RocksDB tree arrives by tar as root, keeping the host's
    uid, so git refuses `rev-parse` (dubious ownership) and build.sh stops. build-freebsd.sh sets
    `safe.directory '*'` in the VM first.
  - Both reruns passed (261117-200358-f31d88, 261117-200402-b01248). The scripts live in
    ~/.scratch/vm-document-gate (ours).
- 2026-11-18 13:22: the Linux VM (pid 85560, booted 13:11) runs another agent's build of
  ~/wsl in it (a `.lock` dir beside the disk); not this quest's job, left alone. It hit the
  6 GiB OOM killer once.
- A5: calibration per value size at the gate's commit, not the row jobs' environment. "Fastest"
  means within 3% of the best geomean of per-workload medians over 5 interleaved trials: below the
  RocksDB A/A spread (0.2-8%), so a pick inside it is noise. The 3% came after seeing one trial at
  0.989; the 5-trial jobs are the test of it.
- A5 re-pin (2026-11-18): 1 KiB moves to lvl-4k-none because A5's rule says so (the pin trails
  by 3.0%, its fillrandom gap 14% outside the spread); this favors RocksDB. Host rows at 1 KiB
  (sync1 YCSB A, F) rerun at the new pin; the 100 B pin stands.

- F1 fix (D107 option (c), 2026-11-17):
  - Backpressure, not a byte bound: `log_state::turn_records` (1, journal seals one record per
    sealer call) caps unsealed records; `join` returns the full record's sequence instead of
    closing it, and the owner waits in `turn_sealer::await_sealed` until the loop seals it. The
    wait wakes at the seal (the sealer lambda calls `sealed()`), not the barrier, so the writer
    fills the next record while the turn writes. Not served: the owner writes the turn inline.
  - A wait that throws (failed turn, closed group) fences the index, as a failed append does.
  - faultfs gains `sync_latency` (each file sync sleeps first), for the lag test's 2 ms barriers.
  - N1: predicate checks `sequence <= retired_locked(group)`. No fails-without test: the window
    is a scheduler race (the waiter must not run between the leave's turn and the reopen), which
    no test seam reaches without a new hook; t6w covers its correctness.
  - N4: t9o stages a version-2 batch, a version-2 checkpoint and a HardState record into the
    group and expects `unsupported_index` at open; replay now refuses a non-batch record past
    the front checkpoint (it skipped them). M-v (the skip) fails t9o.
  - t9o's first version staged the record and opened the index on the same host: a host's
    records() are what it recovered at its own open, so the record was never replayed and open
    succeeded. The code was right; the test now stages, then opens on a new host.

- Sync batch (2026-11-17):
  - A sync batch is one transaction: the fair mapping of RocksDB's WriteBatch (both make a batch
    visible at once and atomically). Gets inside a batch miss its pending writes, as RocksDB's do.
  - The engine moved to a header so a test runs the bench's own code; the IO is injected
    (PosixIO by default, faultfs in the test). The batch test counts the host filesystem's syncs.
  - The many-index row's durable() now applies and awaits every shard (it awaited shard 0 only).
  - A/A on the sync rows as two one-trial jobs, to fit the 10-minute exclusive bound.

- D7, join and leave (2026-11-17):
  - A closed group reopens at open, not at the first write (D6.3 said the first write): journal's
    Store does the same, and an apply never does a durable table update. D6.3 updated.
  - The join's first record waits for the attribute's edit: create joins at its first commit, and
    a logged create_deferred refuses apply (logic_error) until that commit.
  - An index with turn_group opens only with its host (log_mismatch), so no build writes past the
    log; read-only tools decode the head as before. Demotion to unlogged is not built.
  - close() leaves; a failed leave throws before the teardown (the group stays stored, replayed at
    open); the destructor does not leave. kv_strata still destroys without close (crash-like reopen).
  - turn_sealer takes the Host, its loop and identity (Host has no identity accessor; journal
    unchanged). Table calls run under the turn mutex, since close_group calls the driver's forget.
  - Records empty but the log holds the group past the head (the host released it earlier in this
    process): log_lost, "reopen the host", as journal's Store refuses it.
  - The table path refuses a head past the table's record (log_mismatch); invariants make it
    impossible.
  - T7 and T10 measured file bytes through an unlogged open; now through a logged open with nothing
    to replay (same accounting).
  - The A1 mutant set: M-b, M-d, M-n, M-o rerun because their code moved; M-o needed a new T5 case
    (a same-identity host elsewhere), since the attribute check now refuses the old case first.
    Mutants come from `.scratch/mutate.py` via `.scratch/mkmutant.sh` (strata worktree).
  - Row jobs split to fit the 10-minute exclusive bound: a --long job is deferred and proves
    nothing; sync1 runs as three one-trial jobs.

- Step 6, the real seal (2026-11-17):
  - turn_sealer kept as the blocking host loop rather than journal::run: strata's owner thread
    is synchronous and needs inline turns, a serving thread and per-group watchers.
  - Seal mutants rerun at the new commit: M-j, M-k, M-a, M-c, M-h; the others don't touch it.
- Steps 4-5 (2026-11-17):
  - Replay runs inside open() (eager, D6.6); the public replay() is gone. The checkpoint record
    goes through the sealer as a closed record, so the loop alone calls the driver.
  - Checkpoint triggers built: apply's inline flush and checkpoint_log_bytes (default 64 MiB; off
    under defer_io, whose host owns flushes). close() does not checkpoint until D7's leave.
  - M-g replaced by an invariant check (for the reviewer to judge): D5's lease span is not a
    separate mechanism in strata. commit() holds one writer_scope from the log wait to the
    checkpoint record's staging; apply() needs the same lease on the same owner, so a mutant
    "releasing the lease after step 1" has no code to change without adding a second scope that
    nothing calls between. What would break if it did, a batch between X and the record, is
    refused twice: log_state::checkpoint throws unless no record is open or queued and X is the
    record before it, and journal 555b69f's log refuses a checkpoint whose `covers` is behind a
    record (T11 checks the latter). No stepping executor in T1.
  - Extra mutants for claims this step adds: M-m (fail-stop fence), M-n (head older than the
    checkpoint), M-o (checkpoint UUID).
  - M-a is "the checkpoint record made durable before the edit": staged alone it would not reach
    the log before commit returns in a blocking test.
  - A fallback open (`to`) commits its older head before attach_log refuses it with log_lost; the
    next open refuses again (loud). Left as is.
  - kv_strata: logged rows put indexes in <dir>/<i> and the host in <dir>/host (disk counted);
    1 MiB command limit, 4 MiB segments (the shipped default would preallocate 129 MiB).
  - T1 default 8 seeds per crash point (3 s whole test); the job runs 200.

- Edited cmake/check_core_includes.cmake, outside the file set, at the lead's request: journal/turn_driver.h joins the allowlist (journal is already a core library).

- Build launch (steps 1-3):
  - The repin also moves completion to 7731315 and task to 95798a0 (journal 7c08540's pins) and
    sets COMPLETION_TASK ON: journal's turn driver needs completion::task, and strata's
    declarations win FetchContent.
  - Transaction types moved from index.hh to command.hh, so the codec and the index share them.
  - Codec: role and postings bytes decoded raw (stage 1 judges them; a replicated command may be
    invalid), flags/mask/kind strict; unknown kind/version unsupported_index, else corrupt_index.
  - The append runs after publish_cover, so V never passes the cover (D7 rule 4); a throw in it
    fences (an accepted transaction the log lacks).
  - Records stay whole: the transaction that would take a record past command_bytes closes it;
    positions never move between records.
  - Views of a logged index keep their predecessor's metadata/fields/schema/last docid while the
    applied position is past the log (those are not filtered by V); flush() and apply_pending()
    (pending deletes into segment bits, also run by merge()) await the log.
  - Owner-thread reads (index.get under the lease) see applied state; D4 governs reader handles.
  - attach_log refuses a head past the log's durable sequence with log_lost (D6.4, early).
  - T2 and T12 put the log in the index's faultfs directory (no name overlap), so one cut orders
    both; faultfs gains power_on() for a process crash.
  - M-k is T4's flush wait only; apply_pending's wait has no mutant of its own (no deterministic
    publication reaches it).
  - Syntax checks ran locally under `quiet light` (fsyntax-only, older deps); builds are jobs.


- Review fold-in (revision 1), by finding:
  - F1: lease from seal to checkpoint record; V and acks published by a watcher without the lease. Tests T1, T11; mutant M-g.
  - F2: owed verdicts run their chore at replay; no checkpoint until release. Test T7b; mutants M-h, M-i.
  - F3: position taken with the append under the record mutex; sealing by the driver. Test T12; mutant M-j.
  - F4: 64 MiB command limit at admission (replicated ceiling) and a half-segment turn budget. Test T8; mutant M-l.
  - F5: every flush on a logged index awaits the log. Test T4; mutant M-k.
  - F6: T1 writes after recovery (M-b); T10 refuses at runtime (M-f).
  - N1: D7 and D9 follow the host group table and fail-stop.
  - N2: Q1 uses P2, and M3 gains an N-log row.
  - N4: parity claimed for throughput and median only.
  - N6: group ids are never reused.
  - N7: a failed turn reports `outcome_unknown`.
- No strata turn runner: journal main's `TurnDriver::run()` and `journal::run` drive turns.

- Wrote `docs/design/standalone-turn-log.md`, outside the quest's file set, because the lead
  assigned that path.
- Apply-first (log only accepted transactions), not log-first (D2).
- Visibility waits for the barrier on logged indexes (D4).
- Checkpoint waits for the log through its position (I1), accepting a 1-2 barrier writer stall
  per checkpoint until past-generation checkpoints exist.
- Kept Track C's torn-turn rule, rejecting the manifest log's zero classifier for recycled
  segments (D10).
- The doc runs about 3,800 words, over the design-doc ceiling, because the assignment needs
  its tests, jobs and questions spelled out; most of the length is tables.

- A6 VMs (2026-11-17, D105): `ListAgents` is not a tool of this contributor; `ps` at start showed only strata's qemu (pid 62276 linux-vm :18772, pid 91575 freebsd-vm :18774); Detent's :18762/:18763 not running. Never touched them. Mine: ~/.scratch/vm-document-gate/{linux,freebsd} (copies of strata's base.img files, qcow2 overlays, own keys and cloud-init seeds, ports 18782/18784; driver `vm.sh`, jobs `vmrows.sh`, queue `submit.sh`). Linux packages came from cloud-init apt, FreeBSD's from pkg (cmake ninja git python3 bash liblz4 zstd). RocksDB: the pinned 11.11 tree tarred from the host, built in the VM. Memory: host 32 GiB with strata's two VMs resident, so mine are 6 and 3 GiB. Both VMs idle while the other's rows run. Shut both down (`vm.sh linux stop`, `vm.sh freebsd stop`) when the last row job ends, then write the rows into the gate report and perf note from ~/.scratch/vm-document-gate/results/*.

- Lead (11-17 09:30): cancelled the 36 A6 VM jobs queued at 12f59669; landing needs evidence at the landed commit, so the backpressure fix (strata:94d73236) voided them. Resubmit them once at the final commit with ~/.scratch/vm-document-gate/submit.sh. Both VMs are shut down; `vm.sh linux|freebsd start` boots them.
