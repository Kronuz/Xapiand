# turn-log-parity--journal-pin: journal

State: waiting on jobs 261120-174913-536451 261120-174901-9520ed
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

Task 0 of strata `docs/design/rocksdb-parity.md`: journal pin from 9e11713 to journal main
178030b. Work at strata:37197958 on `quest/xapiand/turn-log-parity--journal-pin` (pushed), after
merging strata main (51dcff1e) into the task branch.

## Next

1. Read the four jobs; on green, State "ready for review". On a t5d failure at head, read
   whether the reopen kept a prefix (journal regression) or the test's geometry missed (`end %
   4096 > 400`, the block+1 check).

## Blocked on

Nothing.

## Evidence

Task jobs (no quest criterion: task 0 moves no row):

| Check | Job | Commit |
|---|---|---|
| t5d double damage at 178030b: passed, 15 acknowledged writes, the reopen threw log_lost | 261120-174858-0afc2a | strata:37197958 |
| t5d at the mutant, journal back at 9e11713 (must exit 1) | 261120-174913-536451 | strata:e046c913 (`refs/mutants/turn-log-parity--journal-pin`) |
| strata suite, Release | 261120-174901-9520ed | strata:37197958 |
| turn-log tests, RelWithDebInfo without NDEBUG (standalone_turn_log, barrier_cohort, kv_strata_batch) | `ci:macos-arm64` (CI builds RelWithDebInfo without NDEBUG; local job c6260c cancelled as a duplicate, D57) | strata:37197958 |

## Result

Pending the jobs. Contracts 9e11713..178030b and what each needs from strata:

1. `TurnDriver::Hold` requires a loop (journal 1222e3b): strata builds no Hold (turn_sealer calls
   `commit()` under its turn mutex with no holder), so nothing changes.
2. A suspending `durable()` needs a `post_call` scheduler (485cd23): strata never awaits the
   driver's `durable()`; turn_sealer::await is its own. Nothing changes.
3. Double damage (cca2427, 178030b): damage no crash leaves is reported, and the dropped turns
   cost their groups. New strata test `t5d` in `test/standalone_turn_log_test.cc`: acknowledged
   writes past the head's checkpoint, a flipped fragment and the next block's header; the reopen
   keeps every write or throws `log_lost`.

Already met on main before this pin (group-core moved it to 9e11713): repeat sealer calls with
`turn_records = 1` (`index.hh` `log_state`), the retired floor at the close's sequence + 1 met by
attach's restage at `max(base, retired) + 1` (`index.hh` attach), a failed close in doubt, the
oversize refusal. Idle-park's contract (create/reopen durable at the first turn) is not on journal
main (journal-idle-groups unlanded), so `turn_seal.hh` keeps "durable when it returns".

## Rulings I made

- Merged strata main (51dcff1e) into the task branch first: main already moved the pin to
  9e11713, and the quest branch's cc95ba6 line would conflict with main's otherwise.
- The revert check is a plain job (`test $? -eq 1`) rather than `--fails-without`, which needs a
  criterion tag and no quest criterion covers task 0.
