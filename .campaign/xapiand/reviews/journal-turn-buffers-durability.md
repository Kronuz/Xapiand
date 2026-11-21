# journal-turn-buffers review: durability (with correctness), for Detent as journal's owner

Commit: journal:b88565f0a276c4ea392a171861d8044edd2f1964 (base journal:178030b38f56)
Verdict: APPROVE
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

All at journal:b88565f, logs under `~/.scratch/campaign/jobs/<id>/a1/log`.

- 261120-204135-6afdb1 (bytes, turns, framing, crc fixture): the emitter's three paths match a
  verbatim copy of main's Emitter (test/turn_buffers_test.cc:83-145) over kinds, sizes, start
  positions and manifests up to 4096+ groups; turns roll segments at main's offsets.
- 261120-204136-5db313 warm + seal, with mutants 261120-204136-c5ea74 (seal-copy) and
  261120-204136-0cdfe6 (stage-copy): both fail the test as recorded (FAIL lines in their logs).
- 261120-204136-b4641b forward, with mutant 261120-204136-c0795f (forward-string): fails.
- 261120-204136-af907d power (sector-cut crash at every mutation, forwards and rollovers), plus
  turn_log_power and turn_seal ctest entries.
- 261120-204027-99b762 recycle, with mutant 261120-204028-be128e (drop-recycled): fails.

## What I checked myself

Scripts under `/Users/kronuz/code/Kronuz/Detent/.scratch/detent/review-journal-turn-buffers/`.

- `diff_probe/run.sh`, job 261120-210107-0bdf51 (exit 0): one TurnLog workload built against
  journal:178030b and journal:b88565f; 16 runs (block 512/4096, retain 1-2, small and unbounded
  forward budgets), each with idle groups forwarded, a reopen (forwarding recovered records),
  two parked groups forwarded and read back with live(), then one flipped body byte in the oldest
  segment and 80 more turns and a reopen. Every segment file's size and CRC, the stats, the
  recovered groups, the damage flags and the error text are IDENTICAL on both sides (581 lines;
  43 stats lines with forwards; 5 runs throw "log record changed since it was written" on the
  first turn after the flip, the same turn on both).
- `recycle_probe/run.sh`, job 261120-210314-994981 (ASan+UBSan, exit 0): a sealer replaced
  mid-turn gets nothing back and its replacement gets only its own body; unseal() from another
  thread returns only after a 200 ms recycler call finishes; a recycler that stages fails the
  driver with "a sealer cannot stage records" after its turn is durable; a recycler that
  overwrites its buffer at once leaves the recovered bodies intact over 6 turns; a driver
  destroyed with a turn in flight never calls the recycler and frees cleanly.
- `detent_suite_with_journal.sh` (Detent's campaign `suite` command plus
  `-DFETCHCONTENT_SOURCE_DIR_JOURNAL=$JOB_REF_JOURNAL`, own build directory per revision), run with
  `--quest dense-distribution-build --ref journal=...`: job 261120-205653-b35d8b (journal b88565f)
  and control 261120-205655-e15954 (journal 178030b). Both fail at configure, identically (F1).
- Code read: turn_driver.h (start, completed, recycle, unseal, seal_turn), turn_log.h (stage,
  completed, encode, write, fits, forward_slice, Forward, note, Nodes), framing.h (Emitter,
  Groups, RecordReader, read_record).

Answers to the four questions:

1. No use-after-recycle or aliasing. stage() frames each body into the turn's own buffer
   (turn_log.h:369-375); the driver moves the sealed payload into flight only after stage()
   returns (turn_driver.h:500-501), and nothing reads it again: the write uses Staged::bytes.
   completed() hands it back after log_.completed() (after the host's write and flush), after
   finish() and after resume(), which only posts waiters (turn_driver.h:470-478), so no waiter
   runs inside it. The turn buffer itself returns to spare_ only in TurnLog::completed()
   (turn_log.h:429-430), after the barrier, and the driver resets write_ before reuse.
   unseal/recycle race: the sealer lookup and running_ are set under sealers_mutex_
   (turn_driver.h:699-705), so an unseal either drops it first (payload freed) or waits.
2. Unchanged, including forwarded, recovered, parked and damaged records: the differential probe
   above and the emitter oracle. Recovery is untouched but for Live::bytes; a forward copy of a
   damaged original fails stage() before any byte is written, as main's read_record did, with the
   same message.
3. No leak, double free or lost acknowledgement found: every payload is a std::string owned by
   one Pending; refused, failed and destroyed-in-flight records free with their vectors; a
   recycler's throw is deferred until after finish() and resume() (probe 3 and the recycle case).
4. Not established on Detent, for a reason that predates this branch (F1). The branch changes
   nothing Detent compiles against: Detent includes only journal/memory_io.h and
   journal/mutation.h, neither touched here, and registers no sealer.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | Detent cannot build against journal main at all: its pins (journal e5f6ff1, 105 commits behind 178030b; completion 560c288; task bdafa5f) lack `completion::task`, and journal's CMakeLists sets `COMPLETION_ASIO OFF`, dropping the `completion::asio` Detent links. Same configure errors at base and head, so not this branch's; question 4 stays open until Detent bumps journal. Who acts: the Detent lead (a pin-bump quest: completion d8b538c, task 95798a0, and making completion available before journal so Asio stays on). | jobs 261120-205653-b35d8b, 261120-205655-e15954 (log lines 37-75) | Detent pin bump quest, outside this one |
| F2 | no | gap | `Pending::sealer` keeps every sealed record's Sealer (and its captures) alive until its turn completes, also for sealers with no recycler, so an unsealed sealer's captures now outlive unseal(). Harmless for captured `this`; a captured shared_ptr now releases on the loop at completed(). Owner's call: keep it, or hold a `weak_ptr` and compare with `owner_before`, which keeps the identity check without the lifetime. | turn_driver.h:627,669; recycle_probe case 6: use_count 2 after unseal, 1 after completed() | optional |
| F3 | no | gap | The suite leaves out replace-mid-turn, a cross-thread unseal waiting on a running recycler, and a staging recycler; all hold (recycle_probe cases 1-3). | job 261120-210314-994981 | Fold recycle_probe cases 1-3 into turn_buffers_test recycle |
| F4 | no | gap | No suite test compares a turn that carries forward copies with main's bytes (the Shadow covers record-only turns; forward checks only live() equality). The differential probe shows identical bytes. | test/turn_buffers_test.cc:213-235, 474-510; job 261120-210107-0bdf51 | Optional: a frozen-main shadow for copies, or keep diff_probe as a job |
| F5 | no | nit | Allocation-count checks (warm, seal, recycle, forward) pass vacuously under clang TSan, whose runtime replaces the test's operator new; they prove nothing there, and do not fail. | test/turn_buffers_test.cc:52-61 | Optional: skip them under TSan, or use `__sanitizer_malloc_hook` |

## Not run

- CI run 44867354936 at b88565f was still queued (freebsd, linux-asan-ubsan, linux-tsan) when I
  checked; I relied on no CI result.
- Detent's suite on this branch: it cannot configure (F1); I did not bump Detent's other pins to
  force a build.
- No TSan run of the recycler/unseal race; probe 2 is a timing-based check under ASan only.
- Benchmark numbers in turn-seal.md (allocations and times) were not rerun.
