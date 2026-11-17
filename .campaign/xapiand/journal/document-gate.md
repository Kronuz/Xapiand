# document-gate: journal

State: blocked on lead: D107 step 1 (F1), journal's sealer returns one record per call; options under "Blocked on". A6 and A7 jobs at strata:12f59669 still queued (ids under Evidence), untouched
Quest: .campaign/xapiand/quests/document-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Ready for review at strata:12f59669 (pushed; main is an ancestor, nothing to merge). The sync batch
is one transaction (strata:5e7d1178, 193e6e32); perf note `docs/perf/standalone-turn-log.md` and
gate report `docs/gates/document-profile.md` (strata:eb8437bc); the served test's reader reads at
least once (strata:12f59669, FreeBSD CI's only failure at 193e6e32).

## Next

1. Reruns at strata:12f59669: build-test 261117-075429-907b22, served TSan 261117-075430-571f74,
   T6+t6w+T5 261117-075441-a3837d, t6w TSan 261117-075442-939866, batch test
   261117-075442-e8e160, batch TSan 261117-075443-3e1d5f. CI run 44119676721 (in progress).
2. A7 at 12f59669: fuzz, Homebrew LLVM 23 with ASan+UBSan, 3 workers for 25 min (75 CPU-min)
   each: stored (format 14) 261117-075713-601c02, manifest_log 261117-075726-9eb435,
   manifest_log_file 261117-075737-0c8aec, command 261117-075748-8a784a. Version rejection and
   fixtures (index, format14, manifest_log, directory, standalone_turn_log, corpus replays)
   261117-075800-5c7ffa. Coverage (llvm-cov over every test, per header; llvm-cov keeps one
   binary's counters per inline instantiation, so it undercounts) 261117-075821-5866e7. The
   scale statement is in the gate report (strata:91366374, docs only). ASan/UBSan and TSan: CI's
   linux-asan-ubsan and linux-tsan in run 44119676721.
3. The durability review's findings. Don't change reviewed code unless a rerun fails.

## Blocked on

- D107 step 1 (F1), 2026-11-17: journal cc95ba6 (= journal main) cannot carry several records
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
| A1 | At strata:193e6e32: build-test 261117-055209-094ff8; batch test 261117-055212-dc6fc0 (32 batches: 32 records, 32 barriers, 32 writes); batch TSan 261117-055212-624015; seal TSan (served, T4, T12, T3, T8) 261117-055212-556ecf; t6w TSan 261117-055212-f28b3a; T6+t6w+T5 261117-055213-a1333d. Reruns at 12f59669 queued (Next 1). T1 200-seed sweep 261117-033250-f42f82 (9d7c7710). Killed mutants: M-r 261117-055220-ecd5d0, M-d 261117-052940-986f82, M-p 261117-033229-e6ebfc, M-q 261117-033233-7e4d9b, M-o 261117-033236-b7fbb6, M-n 261117-033244-6cd288, M-b 261117-033248-e735a3, M-j 261117-030347-7f586d, M-k 261117-030357-c294a1, M-a 261117-030405-14df88, M-c 261117-030412-4cd813, M-h 261117-030420-64cdd2, and steps 4-5's M-e 261117-025024-24bfef, M-f 261117-025028-47bf98, M-i 261117-025035-d9bfa2, M-l 261117-025039-d9b68d, M-m 261117-025043-a4a6fc |
| A2 | [artifact] strata `docs/gates/document-profile.md` (eb8437bc), "Prerequisites": format13 76fa3313, shared-reader-views 23e78bb6, write-buffer-reads 2f56d64c, manifest-log 15d39c30, all ancestors of 12f59669 |
| A3 | Durable tiers at 193e6e32, exclusive: sync1 261117-055245-3aecbb, 261117-055253-80fcf5, 261117-055301-8578af; wal1 261117-055310-7264db; A/A 261117-055320-5ca39d, 261117-055327-074989; N-log 261117-034940-b2b9c3 (7db7a919). none tier fits/2x: main 75d1e7e (`core-vs-rocksdb-f14.md`). Owed: 10x rows, none tier at the gate commit |
| A4 | Four axes per row in `docs/perf/standalone-turn-log.md`: sync throughput ties (0.96-1.05x), M2 261117-055334-a41ea3 one barrier per batch. Misses against 1.0x: group commit 1.2-1.45x slower (turn-log-parity A6), memory 2.5-3.4x, disk 3.3x on batched fills, 57-91% more written |
| A5 | RocksDB 11.11 pinned on lvl-16k-lz4 (calibrated fastest for 100 B and 1 KiB) in every row job above; harness per `docs/perf/rocksdb-fairness-audit.md` |
| A7 | Queued at 12f59669: fuzz (75 CPU-min each, ASan+UBSan) stored 261117-075713-601c02, manifest_log 261117-075726-9eb435, manifest_log_file 261117-075737-0c8aec, command 261117-075748-8a784a; version rejection and fixtures 261117-075800-5c7ffa; coverage 261117-075821-5866e7. ASan/UBSan and TSan: CI run 44119676721 (linux-asan-ubsan, linux-tsan), in progress; TSan on the turn log's cases passed (A1). Scale statement: [artifact] gate report "Scale" (strata:91366374) |
| A6 | D105, in progress at strata:12f59669. Linux x86_64 VM (Ubuntu 24.04, 4 vCPU, 6 GiB, qemu HVF, virtio disk on APFS; sync rows are VM-virtio): build 261117-082401-124c5c, 261117-084004-25dd82 (dry); rows 261117-084012-f7f0e7 261117-084020-0646a7 261117-084028-41f69b (fits1) 261117-084037-d4e2de 261117-084045-d42d02 261117-084053-6d8351 (sync1) 261117-084100-27be96 (wal1) 261117-084108-5aea78 261117-084116-1c1ae1 261117-084124-d102e4 (twox1) 261117-084131-c8bd5a 261117-084141-f0823b (big1) 261117-084151-53bbf8 (A/A) 261117-084159-a97142 261117-084208-16902d (A/A sync) 261117-084215-47c83b (N-log). FreeBSD 14.4 x86_64 VM (4 vCPU, 3 GiB): build 261117-085159-309a79, 261117-085213-9c43c9 (dry); rows 261117-085221-047b50 261117-085229-320bfa 261117-085238-8443fb (fits1) 261117-085245-45a977 261117-085253-f20138 261117-085301-985f09 (sync1) 261117-085309-9d6302 (wal1) 261117-085316-28849e 261117-085324-76af0f 261117-085331-4b8d71 (twox1) 261117-085338-2e3ee0 261117-085344-122849 (big1) 261117-085351-5b1072 (A/A) 261117-085359-f66f00 261117-085406-ab3240 (A/A sync) 261117-085413-1ba8b7 (N-log). arm64 smoke: CI run 44119676721 at 12f59669, linux-arm64-gcc build and ctest passed (no dispatch: it would cancel the quest's run). arm64 rows owed before Phase 3R's next gate (D105) |
| A8 | [artifact] strata `docs/gates/document-profile.md` (eb8437bc), in progress: every measured row with commit, platform and jobs, and the owed list |

## Result

strata `docs/design/standalone-turn-log.md` ("Built"); code in `include/strata/core/command.hh`,
`turn_seal.hh`, `index.hh` (logged mode), `test/standalone_turn_log_test.cc` (T2, T4, T9, T11,
T12, served; now T1, t1r, T3, T5, T7, T7b, T8, T10 too), `fuzz/fuzz_command.cc`, fixtures
`test/fixtures/command/`, `bench/kv/kv_strata.cc` (host). Mutant commits on local strata branches
`mutant/document-gate/m-*` (m-j 9ff37a0a, m-k 1f827405; m-a..m-o on 8855d678, applied by
`.scratch/mutate.py` in the strata worktree).

## Rulings I made

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
