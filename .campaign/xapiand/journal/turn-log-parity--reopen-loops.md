# turn-log-parity--reopen-loops: journal

State: ready for review (strata CI run 44862924195 queued at a38d4c26)
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing: the test passes and its mutant fails; CI run 44862924195 on the pushed branch.

## Next

1. Lead: merge strata:a38d4c26 into the quest branch once CI is green. rocksdb-parity.md's gap 4
   (line 731) still cites `kv_strata.hh:288-290` deriving the id; that file is outside this task.

## Blocked on

Nothing. Row 9's refusal was not needed: no case reached a current-generation log at loop >= L.

## Evidence

| Criterion | Evidence |
|---|---|
| A6 (parity row 8, D10 item 2) | 261123-103822-4dd780 at strata:a38d4c26 (exit 0: L 1->3 151 cases over 74 mutations, 136 cut inside the open; L 3->2 117 cases over 57 mutations, 106 inside); fails without: 261120-195114-cec936 at the mutant strata:0a2b2883 (branch `mutant/turn-log-parity--reopen-loops`, reopen derives the group from L: exit 1, 146 failures, every one `log_mismatch`, "the index's turn_group names another host or group") |

## Result

- `bench/kv/kv_strata.hh`: each index's group is picked at create (index i on loop i mod L, as
  before) and kept in `<dir>/<i>/kv-group`; a reopen reads it, never derives it from L. Tests get
  `index(i)`, `group(i)`, `loops()`, and `strata_kv::opening{create, loops}`. A failed turn ends
  its loop thread (the sealer keeps the failure) instead of `std::terminate`, and a throwing
  constructor stops the loops it started.
- `test/test_reopen_loops.cc` (`reopen_loops` in ctest): four logged indexes created at L = 1,
  reopened at 3 and then 2 with a power cut before each reopen; every index read whole after each
  reopen: each acknowledged key with its exact value, each key in its routed index, each index in
  its created group, the host at the new L; unacknowledged `ack=none` tails absent or whole, then
  stable. Each re-partitioning reopen is cut at every mutation under 2 power-loss seeds
  (`--seeds N`), with an acknowledged batch after it, then recovered, written and cut again.
- The test runs 53 s in the job (Release, light; the build before it took 67 s).

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
