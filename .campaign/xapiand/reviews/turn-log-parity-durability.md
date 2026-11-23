# turn-log-parity review: durability

Commit: strata:5953ed4635dda209c95d1b189836a0b51446f752
Verdict: CHANGES
Date: 2026-11-21

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

5953ed46 differs from b533db12 by one include line in `test/test_reopen_damaged.cc`, so job
results at b533db12 carry over.

## Evidence I relied on

- A1: 261121-183106-bb529c (test_ryw held, crash, race; standalone t4, served) and
  261121-183107-df159e (TSan), both exit 0 at b533db12. The mutants (261121-090430-5cd461,
  -090433-fa1d1f, -090436-c180a4) kill owner-at-applied, install-after-log-wait and V-at-barrier
  one by one.
- A3: 261121-183107-decc0f, exit 0 at b533db12: test_large_tx admit, sweep (a power cut under 8
  seeds and a process crash at every mutation of a multi-chunk run, then a close, reopen, writes
  and a second crash), big (strided cuts past 1 MiB and 64 MiB) and visible. 261121-183109-837d25:
  fuzz_command with the chunk kind under ASan+UBSan, exit 0. The mutants (apply per chunk, freeze
  leaves the record open, limit at command_bytes) fail as tagged.
- A4: 261121-183108-f29e13, exit 0 at b533db12.
- Still queued or running when I wrote this, so not relied on: 261121-183142-9fa0f6
  (build-test), -183109-1a0cf0, -183110-47450d, -183110-399fd2, -183128-18f3a2 (A4 mutant),
  -183121-b8a9f9 and -183125-bd57b5 (A2).

## What I checked myself

Scripts are under `strata-wt/xapiand/turn-log-parity/.scratch/xapiand/review-turn-log-parity-durability/`:
`bounded_chunk_probe.cc` with `prelude.inc` (test_reopen_damaged's helpers, on `fl::`), built
and run by `run-bounded-chunk.sh` from test_ryw's compile and link lines.

1. Turn sealing and group commit (`turn_seal.hh`). `turn()` marks a group durable only after
   `turns_.commit()` returns, and only from `turns_.sequence(group)`. The cohort wait in `run()`
   delays the start of the next turn and nothing else. `joined()` and the `owed` state change
   counts and wakes, never `durable`. So `await()` returns only for a durable sequence. The reread
   after an inline turn (`turn_seal.hh:277`, `:300`) throws only after an idle turn that leaves the
   wait unsatisfied. Chunk records carry V − 1 until the last one (`index.hh` `join_chunk`), and
   `seal()` hands one record per group per turn, oldest first, so V passes a large transaction
   only once its last chunk is durable. No group member is acknowledged before its bytes are.
2. Reopen. Every head that a commit installs sits at the end of its record: `commit()` runs
   `await_log()` under the lease, and `seal()` takes the whole open record. So `applied.index >=
   bound` (`index.hh:3391`) proves the head holds every record the damage could take, as the
   journal's lost-bound note requires. Journal `TurnLog::keep` (journal:e4b0100) sets the group's
   sequence to at least the bound, so a bounded group with no records after the bound reopens at
   `staged == bound` and is not refused. `drop_run` drops only a run that the log's end or a
   later record interrupts, and that run was never acknowledged. Finding D2 is the exception.
3. Checkpoint window and merge runs. `log_due` changes only when a checkpoint is triggered.
   Whether a record is live stays the journal's decision through `covers`, and
   `retain`/`reclaim_files` in `commit()` are unchanged. The merge policy's settled runs are
   inert in the index: no caller sets `candidate::range`. Neither path can retire a file that a
   reader or the turn log still needs.
4. Large transactions and read-your-writes. A chunk run is joined under one write lease
   (`lease_scope` refuses a second writer). `log_due` is checked after `apply` returns, so no
   checkpoint lands inside a run, and replay applies the run whole at its last chunk. A3's sweep
   covers every mutation, and A1's jobs cover the owner at applied and views at V.
5. The journal pin. The quest uses `Host::status`, `bound`, `lose`, `GroupStatus::bounded` and
   `records`, which is the contract of journal-lost-bound's `docs/mechanisms/lost-bound.md`.
   journal:e4b0100 is journal main (9cdaa34 minus one fuzz commit) plus lost-bound's six commits.
   Main's pin, 9e11713, is an ancestor of it.

Probe jobs at 5953ed46:

- 261121-213954-ecd99d (`run-bounded-chunk.sh`): this is D2's reproduction. It exits 1 when a
  covered index fails to open. Its cases are a small batch (the control) and damage in the first
  chunk, in a chunk a third of the way in, and in the last chunk.
- 261121-213957-b29133: builds and runs `test_reopen_damaged` at 5953ed46. This is D1's check:
  I expect the build to fail.
- 261121-213849-a92b75: the same probe at b533db12. I cancelled it when the head moved.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | yes | defect | `test/test_reopen_damaged.cc:50,69,75,90,102,108,120,121` still name `faultfs::`. Nothing declares that namespace at 5953ed46: faultlab e3fcfe3's `faultlab/faultfs.h` declares only `kronuz::faultlab`, and `kv_test.hh:20` aliases it as `fl`. `kv_test.hh` already includes that header, so the new include line changes nothing. The suite does not build, and A2 has no evidence at the head (its only passing run, 261121-104705-3d9542, is at strata:a16d117c with journal:1e3505a). The journal's later commits (c851826, e4b0100) change how far a gap's bound reaches, and the test asserts exact bounds. | CI run 44976323378 (`'faultfs' has not been declared`); `git grep "namespace faultfs" 5953ed46` finds nothing; job 261121-213957-b29133 | Rename `faultfs::` to `fl::` as main's faultlab move did, then rerun A2's test and its mutant at the fixed head. |
| D2 | yes | defect | At `include/strata/core/index.hh:3535`, replay throws `log_lost` for a middle or last chunk with no run before it, even when that chunk is at or below the head. A bounded group's records start at bound + 1, which can fall inside a chunk run. This happens when the head covers the run's last chunk, the commit's checkpoint record is not yet durable (test_reopen_damaged's C case), and the damage hits a chunk other than the last. The index then fails `log_lost` on every open, though its head holds everything. It is loud, not a silent loss, but it is the failure Q4 and A2 close: a damaged block failing an index that covers its last record. The same happens after a dropped run's position became the head. | Code path read at 5953ed46; job 261121-213954-ecd99d. If that job exits 0, I withdraw D2. | Check the no-run case only for chunks above the head (skip `at <= cur_.applied` before it). Test: in `test_reopen_damaged`, a C-like index with a chunked transaction whose first chunk is damaged opens with the value, and with a `--fails-without` mutant. |
| N1 | no | gap | Journal lost-bound Limits: tail (rule-5) damage plus a power cut during the first open forgets the bound, and the acknowledged records the damage took are silently gone. The journal brought this in; it was there at journal main before the bound (journal jobs 261121-141538-9a63b7, -141627-fe334f). The quest does not introduce it, but "never silently drop an acknowledged turn" holds only outside this case. | journal:e4b0100 `docs/mechanisms/lost-bound.md`, Limits | Owner's call: land with the limit recorded in rocksdb-parity's A2 section, or wait for the journal to store the verdict before the open record. |
| N2 | no | gap | The journal pin, journal:e4b0100, is not on journal main (journal Blocked on 1). The quest uses only that pin's contract (Q5). | `CMakeLists.txt:38` | Move the pin to journal main once journal-lost-bound lands, then rerun A2. |
| N3 | no | nit | The option `checkpoint_log_bytes` was renamed `checkpoint_floor_bytes` (`index.hh:814`), a public option change where the quest file says "No new public API". Nothing in strata or Xapiand uses it. | `git grep` over strata and Xapiand `src/` | Record it in the quest's contracts or the changelog. |

## Not run

- No local sanitizer or platform builds: TSan, ASan and FreeBSD/Linux are CI's, and the slot's
  sanitizer jobs were still queued.
- A6 measurements and task group-commit's unmerged FreeBSD test fix (strata:6c3d0862).
- The journal's own crash claims (that `commit()` fsyncs before it returns, prefix recovery of
  turns, how far a gap's bound reaches): I took them from journal-lost-bound's reviewed contract.
- D2's dropped-run variant: I reasoned it from the same line and did not probe it separately.
