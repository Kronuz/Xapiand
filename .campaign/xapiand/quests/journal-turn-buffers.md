# journal-turn-buffers: journal's half of D3, the turn path owns its buffers

Repository: journal. Risk: high (durability: the turn path that seals every logged record).
Lens: durability. Detent's lead reviews the diff against journal's authority suite before it lands
(msg-181: build now, land after `detent:journal-idle-groups`).

## Outcome

journal's turn path copies each logged byte once. A record's body is encoded once into its group's
buffer, with room for the batch header, and sealing hands that buffer over without a copy. The
emitter frames the header and payload straight into the turn buffer instead of building
`logical()` first. A forward copy is read into the turn buffer, not into its own string (D2 item 3).
strata's half (turn-log-parity task 2: `index.hh` `log_state`, `turn_seal.hh`) binds to this
contract. Design: strata `docs/design/rocksdb-parity.md`, D3.

## Acceptance criteria

Gate: the suite (`build-test`, CI's macos-arm64) and CI's sanitizers (`asan`, `tsan`).

- A1 The bytes on disk do not change. For fixed inputs, framing straight into the turn buffer
  writes the same bytes as the old `logical()` path: a test comparing both paths over every record
  kind and size class, including a record that crosses a segment boundary, plus the existing
  fixtures unchanged.
- A2 Sealing does not copy a pre-encoded body. With a counting allocator, a warm turn of N records
  from K groups allocates nothing and copies each body byte once: a test with values (allocations
  0, bytes copied equal to the body bytes), plus a `--fails-without` job at a mutant that copies
  the body into a fresh string at seal.
- A3 A forward copy reads into the turn buffer: a test that a forwarded record's bytes equal the
  source's, with no intermediate string (the same counter), plus a `--fails-without` job.
- A4 Crash safety holds: `turn_log_power_test` and `turn_seal_test` pass unchanged, and a power cut
  at every write of a turn built through the new path recovers exactly the acknowledged records: a
  test with values.
- A6 A sealed body returns to its sealer. When a turn completes, the driver hands each sealed
  record's buffer back to its sealer (a `recycle(std::string&&)` beside `Sealer`, in
  `turn_driver.h`), cleared and with its capacity kept, so a warm sealer allocates nothing across
  turns. Evidence: a test with values through the driver (0 allocations over warm turns, and the
  returned buffer is the one the next body uses), plus a `--fails-without` job at a mutant that
  drops the returned buffer.
- A5 [manual] Detent's lead reviewed the diff with its journal tests and ran journal's authority
  suite on it: the reply's message id.

## Contracts

Provides `journal:turn-buffers`: a group-owned encode buffer with header room, a seal that takes it
by move and hands it back when its turn completes (A6), and an emitter that frames into the turn
buffer. The record format is unchanged (A1).
turn-log-parity consumes it.

## Decisions

- Journal's I/O surface and completion ordering stay as Detent's constraints list them (see
  turn-log-parity's quest file); this quest changes only the turn path's buffers.
- No format change: if the copy-free path needs one, stop and ask the lead, since task 3 owns the
  record-format changes (the copy flag and the per-fragment checksum).

- The driver hands sealed bodies back (A6, lead 11-20): D3 reuses one encode buffer per log every
  turn; without the hand-back, strata's log_state allocates a body per turn.
- Lead, 2026-11-21 (contributor's question): the 1,168 B the quest keeps above main after first use
  is a fixed cost per TurnLog, one per host, not per group: it stays, since keeping buffers for reuse
  is this quest's purpose. What must be zero is the per-group slope, which park_count now holds
  (0.565 B per group at journal:87af246, main 0.448 B from host work, job 261121-121340-140534). If
  the 1,168 B turns out to scale with groups or indexes, it is a defect.

## May land early

Nothing: it lands once, after `detent:journal-idle-groups`, with Detent's review.
