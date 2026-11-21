# catalog-stress-cap: journal

State: ready to land (lead, 11-21: all four jobs done as required; CI rerun after merging main; the FreeBSD red was the VM's rsync).
Quest: .campaign/xapiand/quests/catalog-stress-cap.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs on strata:0da17cad: A1 plain 2x25 runs (36bc35, b30f9c), A1 slowed 3 runs (859dd5), A2 mutant (691f38).
Script: ~/.scratch/xapiand/catalog-stress-cap/run.sh (plain|slow|mutant).

## Next

1. Read the jobs; on green, push and set ready for review; then CI on the quest branch (A3).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | plain 50 runs: 261120-234052-36bc35, 261120-234054-b30f9c; slowed (1 ms sleep per two-catalog op): 261120-234057-859dd5 |
| A2 | mutant (budget lets maps reach hard+1), `--fails-without`: 261120-234100-691f38 |
| A3 | ci:CI |

## Result

strata:0da17cad `test/catalog_stress_test.cc`: stress_two no longer relies on the polled sampler
catching a transient peak. After the concurrent phase, at the quiescent point with the sweeps
stopped (nothing demotes), one thread holds request pins on all 64 keys; maps rise until the
budget refuses, and the kernel model's exact peak (`kern.map_peak`) must equal the hard cap (16),
with refusals > 0 and no kernel refusal. The 8-try retry loop is gone; stress_two returns void.

## Rulings I made

- The cap check uses the kernel model's exact high-water mark, not the sampler: reservations are one
  map each and the fill is the only acquirer, so the budget's refusal at 16 means the kernel held 16.
- `t.merges > 0` stays a plain CHECK (no retry): the failing CI runs merged 20-78 times per run.
- The sampled peak stays in the printout only, as a diagnostic.
