# journal-seal-repeat: journal

State: waiting on jobs 261117-092826-f9528f 261117-092848-fe33c7 261117-092857-383352 261117-092907-717875 261117-092917-c2a313 261117-092925-8390f8
Quest: .campaign/xapiand/quests/journal-seal-repeat.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code, tests, mutants and docs committed at journal:6187a32 (branch pushed for CI). Jobs queued;
the six mutant jobs cannot be queued yet (Blocked on).

## Next

1. Read the six jobs; fix any red case.
2. Once the quest file's criteria heading is fixed, submit the mutants (commands below).
3. Check CI on quest/xapiand/journal-seal-repeat at journal:6187a32 (A4), then ready for review.

Mutant jobs, from the home checkout (each fails its case at the mutant):

```sh
for pair in one-call:repeat:A1 framing:repeat:A1 staged:repeat:A1 rotation:repeat_turns:A2 unsealed-again:repeat_unseal:A3 repeat-throw:repeat_throw:A3; do
  m=${pair%%:*}; rest=${pair#*:}; c=${rest%%:*}; a=${rest##*:}
  campaign job submit --quest journal-seal-repeat --for $a --fails-without --what "mutant $m fails $c" \
    --build "sh test/turn_seal_mutants.sh $m && cmake -S . -B build-m -DCMAKE_BUILD_TYPE=RelWithDebInfo >/dev/null && cmake --build build-m -j4 --target journal_turn_seal_test" \
    -- sh -c "test -x build-m/journal_turn_seal_test || exit 127; cd build-m && ./journal_turn_seal_test $c"
done
```

## Blocked on

The lead: the quest file's criteria are under `## Criteria`, but `campaign` reads
`## Acceptance criteria` (campaign.py:2600), so every `--for A1..A5` is refused ("has no criterion
A1 (it has none)") and `--fails-without` needs `--for`. Rename the heading; then the jobs above
can be tagged. The jobs queued so far carry no `--for`.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261117-092848-fe33c7 (`repeat`: three records at staged+1..+3, rooms checked by value, room met and one byte over); mutants one-call, framing, staged pending (Blocked on) |
| A2 | 261117-092857-383352 (`repeat_turns`: `first` across groups, two-group alternation 1,2,1,2); mutant rotation pending |
| A3 | 261117-092907-717875 (`repeat_throw`, `repeat_unseal`, `repeat_threads`); 261117-092917-c2a313 (turn tests under TSan, `repeat_threads` included); mutants unsealed-again, repeat-throw pending |
| A4 | 261117-092826-f9528f (build-test, whole suite with the crash fixtures); 261117-092917-c2a313 (TSan, `-R turn`); 261117-092925-8390f8 (ASan, turn_seal); CI on the pushed branch |
| A5 | [artifact] journal `docs/mechanisms/turn-seal.md` at journal:6187a32: "Repeat calls", "What it gets" (room accounting, `first`), "A failing repeat call", lifetime under "One per group", "Later sealed records" and "Fairness" (no per-group cap) |

## Result

journal:6187a32 `TurnDriver::seal_turn` (include/journal/turn_driver.h) calls a sealer again with
the room left and `first = false` while it returns records, charging each `record_cost` before the
next call; `running_` spans a group's calls and the table is re-checked before each repeat call.
Contract: journal `docs/mechanisms/turn-seal.md`. Tests: `test/turn_seal_test.cc` cases `repeat*`;
mutants in `test/turn_seal_mutants.sh`. Existing call-count checks in `budget`, `trigger`,
`staged_room` and `forget_in_flight` now count the extra repeat call that answers nothing.
Leaves open: CHANGELOG.md entry (outside the file set); telling Detent's lead to repin on landing.

## Rulings I made

- A repeat call that throws or returns a refused record (wrong group, out of sequence, past room)
  stops sealing for the turn, writes and acknowledges the earlier records, and the next turn's
  `start()` fails the driver with that error before calling any sealer. `idle()` is unchanged, so
  the failure comes when a next turn is asked for, exactly as a separate turn's call would throw;
  a waiter on the earlier records is acknowledged (shown under the blocking entry). A throw or
  refusal on a group's first call still fails its turn unwritten.
- Fairness: a group that sealed in the turn claims no `next_seal_` when its repeat call returns
  nothing; the first group not called, or declining its first call under a share, starts the next
  turn. No per-group cap (the room is the cap); a record needing the whole room waits at most one
  extra turn behind a group that sealed first. Stated in the docs.
- `forget` between repeat calls is refused by the existing "records staged" guard, since the
  group's earlier record is pending; the test asserts that rather than a forget-then-skip.
- CHANGELOG.md is outside the quest's file set: left for the lead (suggested text: "`TurnDriver`
  calls a sealer again in the same turn while it returns records, with the room left after each
  `record_cost` and `first = false`; a repeat call's failure leaves earlier records in the turn and
  fails the next turn; no per-group cap").
