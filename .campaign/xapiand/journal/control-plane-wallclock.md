# control-plane-wallclock: journal

State: waiting on jobs 261120-014220-5b29fc 261120-014220-5e1906
Quest: .campaign/xapiand/quests/control-plane-wallclock.md (the lead's; never edit it)

## Doing

Test rewritten (commit 653e4a845): probe wakes over equal 400 ms windows with 0 and 5,000 parked keys
(assert parked*4 >= idle), running()==5000 and fired()==0 while parked, and 5,000 firing keys each fired
exactly once. Lag percentiles printed to stderr, not asserted. Ran locally once under quiet light: 133 checks OK.

## Next

1. Read the two jobs: suite 261120-014220-5b29fc (A1), mutant 261120-014220-5e1906 (A1 --fails-without; script
   ~/.scratch/xapiand/control-plane-wallclock/mutant.sh turns each key's wait into a 1 ms poll).
2. A2 is manual (diff); A3 is CI after the lead pushes.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | suite 261120-014220-5b29fc; fails-without 261120-014220-5e1906 |
| A2 | [manual] no wall-clock assertion left in the test; wakes are counted, lag p50/p99/max only printed |
| A3 | CI after push (lead) |

## Result

Pending jobs.

## Rulings I made

- No loop-turn or handler counter exists in reactor or LagProbe; the property uses the probe's wake count
  (the lead's suggestion) plus exact task counts. The wake comparison is a ratio (4x), not a time bound.
