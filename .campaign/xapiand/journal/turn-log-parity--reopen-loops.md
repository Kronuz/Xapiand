# turn-log-parity--reopen-loops: journal

State: waiting on jobs 261120-194415-bf027b
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

strata:a38d4c26 on quest/xapiand/turn-log-parity--reopen-loops: `kv_strata` keeps each index's
group in `<dir>/<i>/kv-group` (picked at create, read at reopen); `test/test_reopen_loops.cc`.
First run: job 261120-194415-bf027b.

## Next

1. Read 261120-194415-bf027b; fix what it shows.
2. Mutant commit (reopen derives the group from L again, `pick_group(i)`) on branch
   `mutant/turn-log-parity--reopen-loops`; `--fails-without` job at it.
3. Push; record CI.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| parity row 8 (D10 item 2) | test: pending 261120-194415-bf027b; fails without: pending |

## Result

<pending>

## Rulings I made

- The group id lives in a `kv-group` file in each index's directory, written and synced before
  `kv_index::create`, not read from the head's `turn_group` attribute: the harness needs the id
  before the open, and an `index_detail` read of the head is internal. The open still checks the
  attribute against it (`log_mismatch`).
- Create keeps the old spread (index i's group on loop i mod L at create); only reopen changed.
- The host and the four indexes share one faultfs filesystem (prefix views), so one
  `power_off_after` cut orders them all, as `standalone_turn_log_test`'s `host_view` does. A
  directory sync there makes every directory's names durable: it can hide a missing directory sync
  in one index, never invent a loss.
- An acknowledged write must read back with its value; an unacknowledged one (the `ack=none` tail,
  or a batch whose `durable()` threw) may be absent or whole, and stays as the first recovery after
  it found it.
- `kv_strata`'s loop thread catches a failed turn (the sealer keeps the failure; writers' waits
  throw `outcome_unknown`) instead of terminating, and a throwing constructor stops its loops.
