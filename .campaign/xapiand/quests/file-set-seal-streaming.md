# file-set-seal-streaming: file_set::seal checksums a volume streaming, as verify does

Risk: high (durability). The lead writes this quest; contributors read it and never edit it. Its machine
state is in `state.json`: `campaign status file-set-seal-streaming`.

## Outcome

`strata::group::file_set::seal` computes a volume's CRC32C with the streaming `crc32c_of` that
verification already uses (`file_set.hh:149`, called at `:129`), never by reading the whole file into one
string. Sealing a 32 GiB rotated volume or a multi-GiB upload then uses bounded memory, so it cannot fail
with `bad_alloc` and leave a shard that replays the same failing seal forever. Blocking finding R2 of
journal-replaces-wal's durability review (`.campaign/xapiand/reviews/journal-replaces-wal-durability.md`).

## Acceptance criteria

- A1 The suite passes: the `build-test` check, a job tagged `--for A1`
- A2 A test seals a file larger than a bound the test sets on the IO's read size and checks the sealed CRC
  equals the whole-file CRC32C; a mutant that seals by one whole read fails it (a `--fails-without` job)
- A3 One mechanism: `seal` and the verifier share `crc32c_of`; no other whole-file read remains in
  `file_set.hh` for checksums (`git grep` in the journal)
- A4 [ci] CI is green on every platform

## Contracts

None changed: the sealed CRC value is the same bytes' CRC32C.

## Decisions

- Lead, 2026-11-24: its own quest, after group-heal and placement (both change `file_set.hh`).
  journal-replaces-wal moves its strata pin to this landing.

## May land early

Nothing: one change.

## Tasks

None: one contributor (implementation, strong, high).
