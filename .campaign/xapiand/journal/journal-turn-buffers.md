# journal-turn-buffers: journal

State: waiting on jobs 261121-054400-6b0c28 261121-054403-30cfba 261121-054405-8a3d65 261121-054407-4638ad 261121-054409-97d867 261121-054412-bfce5b 261121-054414-82b503 261121-054417-3194fe 261121-054419-ff7741 261121-054429-b6be64 (A1-A6 and the four mutants at journal:a5bfa3e, and a local TSan build) and CI run 44910292621 at journal:a5bfa3e
Quest: .campaign/xapiand/quests/journal-turn-buffers.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixing CI's linux-tsan at journal:5ace07e (run 44869209179): the build failed to link
journal_turn_buffers_test and _probe, "multiple definition of operator new": TSan's static
runtime defines it. journal:d7564f2 leaves the counting allocator out under TSan, as
journal_test does: the test skips its allocation checks and prints so; the probe exits 2.
journal:a5bfa3e merges main (idle groups: park renamed evict; conflicts in turn_log.h only,
resolved to this branch's code under main's names). A1-A6 and the mutants rerun at
journal:a5bfa3e (State). The "3 failed" of the b88565f reruns were the three mutant jobs,
which exit 1 by design (--fails-without); nothing failed there.

Earlier, A6 at journal:b88565f:


Code done at journal:28c0caa (pushed): journal:b7501df framing in place, journal:e2183f9 manifest
reserve, journal:37e371a probe, journal:a210537 and journal:28c0caa notes. Evidence jobs re-run at
journal:28c0caa (State); the same commands passed at journal:e2183f9 (A1 261120-201127-452692,
A2 261120-201129-d86bd2, A3 261120-201131-d1901f, A4 261120-201132-a0e17e; mutants killed:
seal-copy 261120-201148-86ff12, stage-copy 261120-201150-edcde6, forward-string 261120-201152-614139).

## Next

1. A1-A6 reruns and CI (all platforms, linux-tsan included) green at journal:a5bfa3e; then
   update the Evidence table and set State to ready for review.
2. A5 stands at journal:b88565f; since then only the TSan guard (test-only) and the merge of
   main. Tell Detent's lead if the lead wants the merge re-read.

## Blocked on

Nothing. (The hand-back gap: the lead ruled option 1, widened the file set to `turn_driver.h` and
added A6; done.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-204135-6afdb1 (bytes: 4,608 records; turns: 400 turns, 49 rollovers; framing and crc fixtures) |
| A2 | 261120-204136-5db313 (warm: 0 allocations in stage() and completed(), rounds 4-7; seal: 704 bytes for four 128 KiB bodies); fails without: 261120-201148-86ff12, 261120-201150-edcde6, 261120-204136-c5ea74, 261120-204136-0cdfe6 |
| A3 | 261120-204136-b4641b (24 forward copies, 15 warm at 0 allocations, each reads back as its original); fails without: 261120-201152-614139, 261120-204136-c0795f |
| A4 | 261120-204136-af907d (power: 764 crash points; turn_log_power and 23 turn_seal cases pass unchanged) |
| A6 | 261120-204027-99b762 (4 sealers' 128 KiB bodies: warm begin() and completed() 0 allocations, turns 4-7; 28 of 28 later bodies encoded into the returned buffer; 32 returns cleared with capacity, all after the turn is durable; unsealed-in-flight gets nothing; a throwing recycler fails the driver after its turn); fails without: 261120-204028-be128e (drop-recycled: begin() 4 allocations, 524,320 bytes) |
| A5 | manual: Detent's lead reviewed journal:b88565f and ran its probes; APPROVE, no blocking findings (reply to msg-185, 11-20; review in Detent's .scratch/detent/review-journal-turn-buffers/review.md; ASan+UBSan probe 261120-210314-994981, byte-identity probe 261120-210107-0bdf51). F2 documented in docs/mechanisms/turn-seal.md; F1 is Detent's pin wave |

## Result

journal's turn path copies each logged byte once (strata rocksdb-parity D3, journal's half).
- `framing::Emitter` frames a record from pieces straight into fragments: `record(head, body)`
  and `record(head, size, fill)`; it writes into a caller's buffer and hands it back; a
  measuring emitter lays a turn out without writing. `framing::RecordReader` reads a record back
  in order into the caller's buffer (`read_record` uses it). Manifest groups are a flat array.
- `TurnLog::stage` frames each record's stack header and payload into the turn buffer; a forward
  copy streams its original into the copy's fragments, checking its header and size; the turn's
  bytes, notes, manifest and map nodes pass to the next turn.
- Bytes on disk unchanged: main's emitter, kept in the test, writes the same bytes over 4,608
  records and 400 turns with 49 segment rollovers; crc and framing fixtures pass.
- Measured (probe job 261120-201407-998e89, strict, main vs branch, same source): a warm turn of
  32 records, 384,800 body bytes: 122.5 allocations and 1.91 MB allocated before, 0 now;
  stage()+completed() 155-170 us before, 58-61 us now (2.6 times less).
- Notes: journal `docs/mechanisms/turn-seal.md` ("The turn's buffers"), `docs/mechanisms/framing.md`.
- Hand-back (A6): `using Recycler = std::function<void(std::string&& payload)>;` and
  `seal(group, fn, owner = nullptr, Recycler recycle = {})`. The driver keeps a sealed payload in
  flight and, in `completed()` after the log's completion and the waiters' resumption, hands it
  back cleared with its capacity to the recycler of the registration that sealed it (unsealed or
  replaced: freed). A recycler runs as a sealer call (unseal waits, no staging); its throw fails
  the driver after the turn is acknowledged. The driver's own vectors keep their capacity, so a
  warm turn through it allocates nothing in `begin()` or `completed()`. Note: turn-seal.md "The
  turn's buffers", "Handed back".

For Detent's review (A5), once CI is green: diff `main..quest/xapiand/journal-turn-buffers` at
journal:28c0caa; tests: `journal_turn_buffers_{bytes,turns,warm,seal,forward,power}`, unchanged
`journal_turn_log_power`, `journal_turn_seal_*`, `journal_turn_bound_*`, `journal_framing`,
`journal_crc_fixture`, then the authority suite. Mutants: `sh test/turn_seal_mutants.sh
seal-copy|stage-copy|forward-string`.

## Rulings I made

- R9 Under TSan the turn buffers' allocation checks are skipped and say so, as journal_test's
  heap cases do; their other checks still run. Allocation evidence comes from non-TSan builds.

- R6 The recycler is an optional fourth `seal()` argument, after `owner`, not an overload: a
  `seal(g, fn, nullptr)` stays unambiguous, and every existing call compiles unchanged. It is
  keyed to the registration (the sealer's `shared_ptr`) that sealed the record, so a sealer
  unsealed or replaced in flight never sees an old buffer, and `unseal()`'s "no call after it
  returns" covers recyclers too.
- R7 Only sealed payloads stay in flight until completion; a staged record's payload is freed
  after `stage()` frames it, as before, so peak memory for staged records is unchanged.
- R8 A6's window is `begin()` (sealing, the consumer's encode, staging) and `completed()`, from
  the fifth turn: the log's live index takes four checkpoint cycles to fill its spare nodes (the
  case checkpoints every second record, as the warm case does). The write and barrier stay the
  IO seam's (R3).

- R1 "Header room" is the consumer's: the emitter takes the 25-byte record header as a separate
  piece beside the body, so `TurnRecord` and every caller (Journal, Store, Host, the driver,
  Detent) stay unchanged; strata reserves room only for its own batch header.
- R2 `framing::Manifest::groups` becomes `framing::Groups`, a flat sorted-on-read array with the
  map's `operator[]`, brace init and ordered iteration, so turn_seal_test compiles unchanged and a
  block's manifest allocates nothing once warm.
- R3 A2's "warm turn" window is `TurnLog::stage()` and `completed()`; the write and barrier are
  the IO seam's (faultfs allocates there). Through the driver (`seal`), `begin()` includes the
  driver's own bookkeeping vectors (outside this file set), so that case bounds bytes allocated
  (under a quarter of one body) rather than counting zero.
- R4 Map nodes freed by the turn path (`next`, the live index) are kept for reuse without a cap:
  never more than the map held at its largest, so steady checkpoint cycles allocate none.
- R5 `Live` keeps the payload size in its padding (`bytes : 56`, sizeof 32 unchanged) so a forward
  copy's layout is known before it is read; the copy now also checks its original's record
  header (kind, group, sequence) and size, as `live()` does.
