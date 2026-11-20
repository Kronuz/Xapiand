# document-gate: journal

State: ready for review: build-test 261120-065738-302256 green at strata:d956c0f4; the lead pushes (CI sanitizers, A7) and lands
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

2026-11-20, D143 (owner: land on the existing numbers; criteria rewritten at Xapiand:e169d08f4).
No measurement reruns. strata:d956c0f4: main already merged (origin/main e3edb426 is an ancestor
of 1bf96fbd, "Already up to date"; no conflicts). `docs/gates/document-profile.md` rewritten from
the dd02c63d rows (every set with its jobs, every loss against RocksDB with its magnitude, the
VM rows, the A7 jobs, scale) and names turn-log-parity A6 as where the losses close;
`docs/perf/rocksdb-fairness-audit.md` gained "RocksDB's pin per value-size class" (A5). The
Evidence table below is in the new shape. Removed `.campaign/xapiand/reviews/document-gate-
durability.md` from this branch (identical on master); `-design.md` is not on master, left.
The 1bf96fbd merge touched no header kv_strata includes, so the dd02c63d rows describe the
landing code; the Doing entry below it is the earlier plan, superseded by D143.

2026-11-20 04:30, fresh session. Every dd02c63d job ended (29 done, 12 cancelled by the lead,
msg-2). Merged main into the branch: strata:1bf96fbd (main's 6 files: catalog.hh, its test and
mechanism notes, CMakeLists.txt, ci.yml). No header under include/ or bench/kv includes
catalog.hh, so kv_strata's source is the one dd02c63d and 723a5e97 timed; the job identity is the
tree, so every evidence job must run again at 1bf96fbd. First submission (the A1/A7 reruns) was
denied by permissions; I stopped there. Ready to submit, in order, once allowed:

1. `campaign job rerun` of A1 (eec44f, dc03a4, 564919, add0a0, 63c793, abff7f) and A7 (0f74fd,
   78c0da, 438cd8, abafdb, 90ce4e, a1dd1c).
2. Strict pass, one trial per set: `.scratch/xapiand/document-gate/rows-1bf9.sh` (home checkout):
   calibrate 100 and 1024 (5 trials, A5 rule unchanged), sync1, wal1, N-log (1 trial), fits1,
   twox1, big1, tenx1.
3. A6 per VM: build (light) -> dry (light, --after) -> short.sh (exclusive, --after), as the dd02
   specs; 2 exclusive jobs, the only exclusive ones.

Read at dd02c63d (trial a, exclusive; ratios strata/RocksDB, pooled with the 723a5e97 trials of
the same engine source as the variance record; `pertrial.py` in the same scratch dir):

- none tier throughput: faster in every trial of every row. fits1 1.09-2.95x (trial a), twox1
  1.91-6.97x, big1 1.60-4.03x, tenx1 1.49-3.96x. Lowest single trial over 4: big1 seekrandom 1.03,
  fits1 readrandom 1.06, fits1 fillseq 1.09.
- none tier "to durable": fits1 overwrite 0.79-0.96 over 4 trials (1.13x slower, a miss);
  fits1 ycsb_a 0.92-1.07 (tie); fits1 ycsb_f 1.00-1.34; every other row faster.
- sync1 (A/A record 261117-055320-5ca39d: sync/1 spread 0.2-0.4%, sync/100 strata 2.3%):
  sync/1 fillrandom 0.98-1.00 and YCSB F 0.98-0.99 over 4 trials (cv 0.4-0.6%), so strata
  trails by 1-2% outside the A/A band; YCSB A 0.98-1.05 and sync/100 0.98-1.11 tie.
- wal1 1.33-1.69x faster (6 trials over two commits 1.30-2.08x); to durable 0.99-1.55.
- N-log: K=4 on 1 log 103 vs 143 ops/s (1.39x slower, 2 trials, a miss); K=1 and K=8 tie (0.96-1.00).
- Other axes, misses: tenx1 p99 1.19-2.08x higher on fillseq, fillrandom, overwrite; tenx1 CPU
  1.97x (fillseq) and 1.42x (overwrite) more; peak memory up to 2.3x (10x rows) and 3.4x (sync
  1 KiB); disk 39.5x on sync/1 fillrandom, 3.7x on wal1; bytes written 36-92% more (sync).
- A5: both pins are the fastest at dd02c63d: 100 B lvl-16k-lz4 (next lvl-4k-none 0.972),
  1 KiB lvl-4k-none (next lvl-16k-lz4 0.980).
- A6 rows at dd02c63d, Linux build passed this time: Linux sync1 1.36-2.02x faster, wal1
  1.15-1.35x slower; FreeBSD sync1 1.21x faster to 1.12x slower, wal1 1.31-1.38x slower. Both
  VMs stopped (`vm.sh status`).

No row is close enough for exclusive trials to change its verdict: every throughput row clears
1.0 in every trial; the misses (overwrite to durable, K=4, sync/1's 1-2%) hold in every trial;
the two straddling rows (fits1 ycsb_a to durable, cv 6%; sync YCSB A, cv 3%) would need about 35
and 10 trials to resolve a 2% gap, so they stay ties under the A/A rule.

## Next

1. build-test 261120-065738-302256 is green (A1, A7); the lead pushes, so CI's sanitizers
   (A7) run at the landing commit.
2. For the lead: `.campaign/xapiand/reviews/document-gate-design.md` is on this branch only
   (absent on master): land it or drop it. `document-gate-durability.md` was removed here.
3. For the lead: a durability re-accept against the current criteria (brief lists it).
4. For the lead: 1 KiB fillrandom (none) runs 0.78x RocksDB at 256 MiB because strata flushes
   inline on the writer (profile 8c6f59); a background flush is an engine change, so a follow-up
   quest after this gate lands.
5. Packing follow-up once Detent's journal change lands: drop `turn_records` = 1, measure wal1.

## Blocked on

- (none: D143 resolved the A3/A4 landing question; the permission block and the strict-versus-
  exclusive question are moot with no reruns.)
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

- D143 (lead, 2026-11-20): followed; see Doing. No rerun, no strict or exclusive job, no bench/kv
  run; only the build-test verify.
- msg-2 (lead, 2026-11-19 23h, trial policy): followed. One trial per row at dd02c63d decided
  every row; none had a trial-a margin inside its noise that more trials could resolve (Doing:
  the misses hold in all 4 trials, the two straddling ties need 10-35 trials for a 2% gap). So
  no trials b/c and no A/A resubmitted; the A/A record 261117-055320-5ca39d/-074989 stands. At
  the merged head: one strict trial per set; exclusive only for the two A6 VM jobs.
- Durability re-accept (bench/kv/run.py since cbf08ebf, commits a93f2b59 and dd02c63d): run.py
  gained multi-trial calibration (`calibrate OUT SIZES TRIALS`, geomean of per-workload medians,
  exit nonzero when a pinned `KV_VARIANT_<size>` trails the fastest by over 3%) and the `tenx1`
  set (fits1's rows at big1's 10x scale). No durability tier, row definition of sync1/wal1/N-log,
  kv_strata host, or turn-log code changed; the durability claims do not depend on it. Re-review
  is the lead's call.
- Merge (2026-11-20): main merged into the branch at strata:1bf96fbd, not pushed (the lead pushes).

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

Criteria as rewritten at Xapiand:e169d08f4 (D143). The gate report and the audit are committed in
strata at d956c0f4; the rows' jobs ran at dd02c63d (same `kv_strata` source as the merged head
1bf96fbd), cited in the report. build-test is the one job at the landing tree.

| Criterion | Evidence |
|---|---|
| A1 | tests in the build-test check: 261120-065738-302256 (strata:d956c0f4). |
| A2 | [artifact] strata `docs/gates/document-profile.md` (d956c0f4), "Prerequisites": format13 76fa3313, shared-reader-views 23e78bb6, write-buffer-reads 2f56d64c, manifest-log 15d39c30 |
| A3 | [artifact] strata `docs/gates/document-profile.md` (d956c0f4), "Rows measured": fits1, twox1, big1, tenx1, sync1, wal1 and group-commit rows at dd02c63d, each with its job ids in the report |
| A4 | [artifact] strata `docs/gates/document-profile.md` (d956c0f4), "Losses against RocksDB": every loss with its magnitude (K=4 1.39x slower, fits1 overwrite to durable 1.14x slower, p99.99 wal 130-193x, peak memory up to 3.4x, disk up to 39.5x, bytes written up to 3.2x, CPU up to 1.97x); the report names turn-log-parity A6 as where they close |
| A5 | [artifact] strata `docs/perf/rocksdb-fairness-audit.md` (d956c0f4), "RocksDB's pin per value-size class": 100 B lvl-16k-lz4, 1 KiB lvl-4k-none, each the fastest over five interleaved trials at dd02c63d; jobs in the audit |
| A6 | [artifact] strata `docs/gates/document-profile.md` (d956c0f4), "VM rows": Linux and FreeBSD dry run and VM-virtio sync1+wal1 as measured at dd02c63d (D114); arm64 owed before Phase 3R's next gate (D105); jobs in the report |
| A7 | tests in the build-test check: 261120-065738-302256 (strata:d956c0f4); ASan, UBSan, TSan on CI at the landing commit (the lead pushes). |
| A8 | [artifact] strata `docs/gates/document-profile.md` (d956c0f4): every row with its commit, platform and jobs, the pins, the A7 checks, the scale |

Supporting for A1 (ran at dd02c63d or at mutant commits, not the landing tree): Earlier, at dd02c63d: build-test 261119-165623-eec44f; T1 200-seed sweep 261119-165631-dc03a4; T6 sweep and t6w 261119-165631-564919; kv batch test 261119-165631-add0a0; TSan 261119-165631-63c793, batch TSan 261119-165632-abff7f. Fails without (at their mutant commits): M-s 261117-122123-3b0f9a, M-t 261117-122132-b7e7f7, M-v 261117-180849-82b11b, M-r 261117-055220-ecd5d0, M-d 261117-052940-986f82, M-p 261117-033229-e6ebfc, M-q 261117-033233-7e4d9b, M-o 261117-033236-b7fbb6, M-n 261117-033244-6cd288, M-b 261117-033248-e735a3, M-j 261117-030347-7f586d, M-k 261117-030357-c294a1, M-a 261117-030405-14df88, M-c 261117-030412-4cd813, M-h 261117-030420-64cdd2, M-e 261117-025024-24bfef, M-f 261117-025028-47bf98, M-i 261117-025035-d9bfa2, M-l 261117-025039-d9b68d, M-m 261117-025043-a4a6fc, and 261116-211626-0daa5f, 261116-211632-9ff248, 261117-025010-4c8220, 261117-025013-a6cec4, 261117-025017-0b218d, 261117-025020-93f7da, 261117-025032-8fc28b, 261117-025046-ec1d6e, 261117-025050-ee8915

Earlier runs at dd02c63d, not evidence for any criterion: fuzz stored 78c0da, manifest_log 438cd8, manifest_log_file 0f74fd, command abafdb (75 CPU-min each; the CPU-hour is deferred, `--later`); version rejection and fixtures a1dd1c; coverage 90ce4e

Cancelled by the lead (msg-2), not evidence: 165701-4be574, 165702-c1bb81, 165706-d9da0c,
165707-20d6c9, 165710-9f6292, 165711-9c2bc2, 165714-acce73, 165715-e05e4d, 165718-9eddad,
165719-85a4ae, 165722-afc856, 165723-3435c8.

## Result

strata `docs/design/standalone-turn-log.md` ("Built"); code in `include/strata/core/command.hh`,
`turn_seal.hh`, `index.hh` (logged mode), `test/standalone_turn_log_test.cc` (T2, T4, T9, T11,
T12, served; now T1, t1r, T3, T5, T7, T7b, T8, T10 too), `fuzz/fuzz_command.cc`, fixtures
`test/fixtures/command/`, `bench/kv/kv_strata.cc` (host). Mutant commits on local strata branches
`mutant/document-gate/m-*` (m-j 9ff37a0a, m-k 1f827405; m-a..m-o on 8855d678, applied by
`.scratch/mutate.py` in the strata worktree).

## Rulings I made

- 2026-11-20: a row needs more trials only when its pooled trials straddle 1.0 and a feasible count
  (n = (1.96 sd / gap)^2, at most 3 jobs) resolves it; none does, so no exclusive trials. The
  723a5e97 trials count as variance for dd02c63d: the engine source is the same (run.py only).
- 2026-11-20: the strict pass uses 1 trial for wal1 and N-log (were 3 and 2): their margins are
  30-100% against a cv of 7-11%. Calibration keeps 5 trials, A5's fixed rule.
- 2026-11-20: sync/1's 1-2% deficit is a miss, not a tie: the harness's 10% tie band is wider
  than this row's A/A (0.2-0.4%), and measurement.md counts differences against the A/A.

- 2026-11-19 (fresh session): measured rows resubmitted rather than `rerun`, so their output
  directories carry dd02c63d, not the old commit; fits1 and twox1 run as three one-trial jobs
  (their three-trial jobs took 507-556 s of the 600 s bound). The A/A now runs 1 KiB on the pin
  (lvl-4k-none) as every row does; it ran on lvl-16k-lz4 before. Superseded jobs in the old
  rerun list (M2-M4 probes, pre-fix TSan, old-pin sync1, build-tests at older commits) are not
  rerun: the journal cites their replacements.

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
