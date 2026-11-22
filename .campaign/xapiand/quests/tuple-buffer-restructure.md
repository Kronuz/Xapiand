# tuple-buffer-restructure: prospect's tuple_buffer: run building, the entry codec and the run merger out of the class body

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status tuple-buffer-restructure`.

## Outcome

prospect's `include/prospect/tuple_buffer.hh` is 1,229 lines, and `class tuple_buffer` (line 203)
is 1,024 of them: the write buffer from ingest to runs. Its pipeline stages are apart already:
- ingest and flush planning (`add`, `flush`, `plan`, `field_plan`, `splitters`) stay in the class;
- run building (`drain`, `group_run`, `split`, `sort_groups`, `radix_sort`, `sort_run`, batch
  memory) moves to `tuple_buffer_run.hh`;
- the entry codec (`entry_view`, `encode_entry`, `decode_entry`, `decode`, `relabel`) moves to
  `tuple_buffer_codec.hh`;
- the k-way `merger` (a nested class, defined outside the body) and the read side (`scan`,
  `each_term`, `postings_of`) move to `tuple_buffer_read.hh`.
Pieces under about 100 lines join a neighbor. No open branch edits the file (prospect follows the
release, D39), so a move now costs no one their edits.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, and CI on every platform ([ci])
- A2 The move changes no code: the `members` check passes, and the journal answers each warning.
  A Release `tools/fnsizes.sh` diff of the test binaries, base against head, in one job, shows no
  function whose size changed, or each difference explained in the journal
- A3 Every new header compiles alone: `tools/selfcontained.sh` over the new headers
- A4 [artifact] `docs/source-layout.md` lists each header and its stage

## Contracts

prospect's public API and the run format's bytes stay the same.

## Decisions

- Owner, 2026-11-21, on the class bodies over 1,000 lines: "These files would have to be carefully
  split. I would still split them with craftmanship and aiming at improving maintainability and
  parallel work. Decide which are worth and how to split them cleanly."
- Lead, 2026-11-21: worth it now because it is free: no branch edits it. It is the one write buffer
  (one mechanism per kind) the engine keeps, and its codec is the format the runs carry. Low risk:
  a proved move.

## May land early

Nothing: one landing.
