# turn-log-parity--ryw: journal

State: waiting on jobs 261121-090415-53357a 261121-090430-5cd461 261121-090433-fa1d1f 261121-090436-c180a4 261121-090438-28fbcd
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

A1's test and its mutants, queued at strata:d16abf5a (branch quest/xapiand/turn-log-parity--ryw).

## Next

1. Read the five jobs; fix test_ryw if a build or the race's `behind > 0` misfires; rerun.
2. Push the branch; hand to the lead for the task merge.

## Blocked on

Nothing for A1 in strata. For the lead (outside this task's files): rocksdb-parity's open Q5. A
client whose reads go through views, as Xapiand's do (`src/engine/strata/shard.cc` hands readers
`index->reader()`), still sees its `ack=none` write only after the barrier. strata's contract is
met (lease holder at applied, views at V); closing it for Xapiand's clients is Q5's option (a),
route a session's reads after its write to the owner, a Xapiand change. Recommendation: (a), as
the design note defaults, as its own Xapiand task.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261121-090415-53357a (test_ryw held, crash, race; standalone t4, served) and 261121-090438-28fbcd (TSan: test_ryw, race x2 more); fails without: 261121-090430-5cd461 (owner-cut-at-v), 261121-090433-fa1d1f (install-before-log-wait), 261121-090436-c180a4 (v-follows-apply) |

Mutants (scratch refs in strata, each one commit on strata:d16abf5a):

- `refs/scratch/ryw/owner-cut-at-v` (strata:eea4f677): the owner's get reads the buffers at V.
  Kills: held (owner reads), race (`missed`).
- `refs/scratch/ryw/install-before-log-wait` (strata:f1fd42d3): `await_log()` empty. Kills: held
  (the forced refresh returns before the barrier and installs the write).
- `refs/scratch/ryw/v-follows-apply` (strata:ca0b99f2): V stored at apply (Q3's option 2).
  Kills: held (views see the write), race (`exposed`).

## Result

strata already holds A1's contract at the lease holder: `get`, `find_key`, `find_docid` and
`scan` read the active and frozen buffers and the segments with no V cut (`index.hh`, `get`,
`lookup_key`, `locate_docid`), views read at V (`reading`, `key_at`), and every install and
pending delete waits for the log (`await_log` in `flush`, `commit`, `apply_pending`). No
mechanism changed; the task adds the test that pins it (`test/test_ryw.cc`, ctest `ryw`):

- held: barrier held (served sealer, the test writes turns). After an `ack=none` write (a
  replace of a segment's key, a delete of another, a new key) the owner reads all three, by get
  and scan, while `log_synced < applied`; a live read and a view acquired after the write read
  the durable state, V unchanged. A refresh forced during the barrier parks in the log wait, the
  views unchanged and no segment installed; after the barrier V moves by one, both read the
  write, and the segment installs.
- crash: the same write, then a power cut (process crash and 3 seeds): recovery holds the
  durable state, so what views hid is exactly what the cut removes.
- race: served loop, 300 µs barriers, 3000 writes; the owner reads each at once, a reader thread
  checks every value it sees is durable (sealer's sequence ≥ the write's position) and counts
  reads behind the owner (must be > 0: the window was exercised).

## Rulings I made

- No strata mechanism change: the owner already reads at applied, so the "fix" A1 names has no
  commit without it; the mutants stand in for "without the mechanism", one per claim (owner at
  applied, installs after the log wait, V only at the barrier).
- The barrier is held by not serving turns (a served sealer whose turns the test writes, as
  standalone T4), not a faultfs sync hook: faultfs has none, and the hold is deterministic.
- `standalone_turn_log_test t4 served` ride in the branch job as the neighboring V checks.
