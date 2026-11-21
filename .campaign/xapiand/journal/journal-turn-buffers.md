# journal-turn-buffers: journal

State: waiting on jobs 261120-201735-dfb864 261120-201737-6e4864 261120-201739-07500d 261120-201741-5c74c1 261120-201744-3bc198 261120-201746-56b121 261120-201748-82c5c4
Quest: .campaign/xapiand/quests/journal-turn-buffers.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done at journal:28c0caa (pushed): journal:b7501df framing in place, journal:e2183f9 manifest
reserve, journal:37e371a probe, journal:a210537 and journal:28c0caa notes. Evidence jobs re-run at
journal:28c0caa (State); the same commands passed at journal:e2183f9 (A1 261120-201127-452692,
A2 261120-201129-d86bd2, A3 261120-201131-d1901f, A4 261120-201132-a0e17e; mutants killed:
seal-copy 261120-201148-86ff12, stage-copy 261120-201150-edcde6, forward-string 261120-201152-614139).

## Next

1. Paste `campaign brief`'s table once the jobs at journal:28c0caa finish.
2. CI (macos-arm64, linux-asan-ubsan, linux-tsan) green at journal:28c0caa.
3. Then: ready for Detent's review (A5), with the commit and tests below.

## Blocked on

Nothing blocks A1-A4. One gap for the lead, outside this file set: strata's half wants its body
buffer back for reuse (D3, "one encode buffer per log, reused every turn"). The driver moves the
sealed `TurnRecord` into `pending_`/`flight_` and frees its payload when the turn completes, so a
strata log_state that hands over its body by move allocates a new body each turn. Options:
1. Widen this quest's file set to `turn_driver.h` for a small hook: after `stage()`, hand each
   sealed record's payload back to its sealer (for example a `recycle(std::string&&)` beside
   `Sealer`), cleared, capacity kept. Recommended: it is the last allocation on the sealed path.
2. Leave it to turn-log-parity task 2 to keep two buffers and accept one allocation per turn.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-201735-dfb864 (bytes: 4,608 records; turns: 400 turns, 49 rollovers; framing and crc fixtures) |
| A2 | 261120-201737-6e4864 (warm: 0 allocations in stage() and completed(), rounds 4-7; seal: 704 bytes for four 128 KiB bodies); fails without: 261120-201148-86ff12, 261120-201150-edcde6, 261120-201744-3bc198, 261120-201746-56b121 |
| A3 | 261120-201739-07500d (24 forward copies, 15 warm at 0 allocations, each reads back as its original); fails without: 261120-201152-614139, 261120-201748-82c5c4 |
| A4 | 261120-201741-5c74c1 (power: 764 crash points; turn_log_power and 23 turn_seal cases pass unchanged) |
| A5 | <no evidence yet> |

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
- Open: the driver frees a sealed payload at completion; returning it to the sealer is outside
  this file set ("Blocked on").

For Detent's review (A5), once CI is green: diff `main..quest/xapiand/journal-turn-buffers` at
journal:28c0caa; tests: `journal_turn_buffers_{bytes,turns,warm,seal,forward,power}`, unchanged
`journal_turn_log_power`, `journal_turn_seal_*`, `journal_turn_bound_*`, `journal_framing`,
`journal_crc_fixture`, then the authority suite. Mutants: `sh test/turn_seal_mutants.sh
seal-copy|stage-copy|forward-string`.

## Rulings I made

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
