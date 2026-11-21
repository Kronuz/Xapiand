# journal-turn-buffers: journal

State: waiting on jobs 261121-113430-26daa5 261121-113432-44c9c7 261121-112940-8f72c2 261121-112943-a14c94 261121-112945-a24d08 261121-112947-e0956c 261121-112949-d67fc2 261121-113000-0950f8 261121-113002-fb2cfa 261121-113004-e6c020 261121-113006-d7b0ff 261121-113008-473d2b 261121-113018-351103 261121-113020-69fb20 (park_count with and without the fix, A1-A6 and the four mutants, TSan, Detent's two review probes, all at journal:5a5972d or the fix's parent) and CI at journal:5a5972d
Quest: .campaign/xapiand/quests/journal-turn-buffers.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fixing CI at journal:6cdd923 (run 44927231788): main's park_count failed on every platform,
"a parked group costs the host no heap: 137.0 B per group" (Linux), 129.0 (macOS, FreeBSD).
Cause (from the run's numbers): the driver rotated flight_ with spare_flight_, two in-flight
vectors; park_count's second 10,000-group batch grew the second one. The heap grew 1,370,177 B
(Linux) and 1,290,152 B (macOS): 10,003 Pending entries of 136 and 128 B (std::string is 32
and 24 B there; 80,025 B apart, 8 B each) plus about 9.8 KB. journal:5a5972d keeps one in-flight
vector: completed() moves flight_ out and hands it back cleared, unless a turn begun meanwhile
took one. Nothing per group: what the driver keeps is sized by its largest turn, once.

Jobs: park_count at journal:5a5972d 261121-113430-26daa5; --fails-without at journal:6cdd923
261121-113432-44c9c7 (target journal_journal_test; the first pair, 261121-112935-470d0a and
261121-112938-b90e6c, named journal_test and never built). Every rerun configures with -G Ninja into build-ninja (the slot's build
is Makefiles); Detent's probes run with CMAKE_GENERATOR=Ninja, the differential one against
main journal:335b1b5 (was journal:178030b).

For the lead, since the durability review accepted journal:5ace07e:
1. journal:d7564f2: test only. turn_buffers_test skips its allocation checks under TSan and
   says so; turn_buffers_probe exits 2 there (TSan's runtime defines operator new).
2. journal:a5bfa3e merges main (idle groups). turn_log.h: three conflicts, resolved to this
   branch's code under main's names (parked_ -> evicted_, Parked -> Evicted); the copy-replace
   path keeps `Live{..., n.bytes}`. The rest of turn_log.h's change since journal:5ace07e is
   main's own (evict/peek/idle, forward_scanned, damaged_fragment's length check).
3. journal:6cdd923 merges main again: no conflicts; no change to turn_log.h, turn_driver.h or the
   turn_buffers tests.
4. journal:5a5972d (this fix): turn_driver.h completed() and the member list only; turn_log.h
   and the turn_buffers tests unchanged. turn-seal.md "The driver's own buffers" updated.
Confirm by job: `git -C <journal> diff 5ace07e 5a5972d -- include/journal/turn_driver.h`
(6 lines of the fix plus main's), park_count 261121-113430-26daa5 and 261121-113432-44c9c7,
A6 261121-112949-d67fc2 and its mutant 261121-113006-d7b0ff.

## Next

1. When the jobs and CI at journal:5a5972d pass: update the Evidence table with them and set
   State to ready for review. If park_count still fails, the ~9.8 KB remainder is next.
2. A5: Detent's probes rerun at the head (261121-113018-351103, 261121-113020-69fb20); the
   lead decides whether Detent's lead re-reads the merge and the fix.

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

- R10 The driver keeps one in-flight vector, not a rotating pair: a pair keeps two buffers of the
  largest turn, and which one a turn grows depends on the parity of the turns before it. A turn
  begun during completed() (a waiter's) takes a fresh one; this turn's is then freed.

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
