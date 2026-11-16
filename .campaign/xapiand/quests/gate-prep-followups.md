# gate-prep-followups: Carried follow-ups: root routes, the WAL view, fuzz_lua

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status gate-prep-followups`.

## Outcome

The follow-ups road-to-release carried from gate prep (Xapiand:11e3d82a4) and from X4-R1
(Xapiand:8ef0f8633) are fixed: commands at the root answer what they mean, `test_quarantine` cleans
up, refused names answer 4xx instead of 500, `:wal` stops building the whole log in memory, and
`fuzz_lua` agrees with itself on a cyclic document.

## Acceptance criteria

- A1 `HEAD /:stats`, `/:schema` and unknown commands at the root answer what a command at the root means, not 200 (carried "Gate prep landed", follow-ups): a test in the `build-test` check and a docs E2E example
- A2 `test_quarantine` leaves no directories behind (carried, follow-ups): the `build-test` check
- A3 APFS-refused code points in an index name answer 4xx, not 500 (carried, follow-ups): a test
- A4 `PUT /.xapiand/node` answers 4xx naming the reserved name, not 500 with an internal message (carried; gate-prep ledger Next): a test
- A5 `:wal` no longer builds the whole log as one `MsgPack`; it streams or bounds it, and answers a shard path (`/idx/.__N/:wal`) for an index of several shards (carried; gate-prep ledger Next): a test with a log larger than the bound
- A6 The pinned `fs` carries the quarantine path-buffer fix (`fs:291842c` on `fix/quarantine-path-buffer`) and `xapiand_test_quarantine` passes on it (gate-prep ledger Next): the `build-test` check
- A7 `fuzz_lua` seed 7's cyclic document: the oracle and the script agree (too large against error), pre-existing on X4 (carried "X4-R1 landed", follow-up; x4-r1 ledger Next: the fuzz oracle's cycle versus byte-budget class): `fuzz_lua` replaying seed 7 in the `build-test` check
- A8 Docs E2E on strata: every assertion passes, 0 regressions: the `e2e` check

## Contracts

The HTTP routes at the root and `:wal` keep their documented shapes; the answers for the refused
cases become 4xx.

## Decisions

- Open (lead): whether `fs:291842c` was pushed and pinned after gate prep (the ledger's Next asked
  the lead to); A6 holds it either way.
- The WAL stays until 7b (D13): A5 bounds the view, it does not replace the WAL.

## May land early

Tests.
