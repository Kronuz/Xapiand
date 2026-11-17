# name-index: journal

State: waiting on jobs 261117-110158-de9685 261117-110158-9cdd8c 261117-110204-e7835e 261117-110254-f71df8 261117-110258-76c693 and 9 mutants
Quest: .campaign/xapiand/quests/name-index.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead findings at b43d399 fixed in xapiand:8987bea (master merged: xapiand:291ec89):
1. async_check: the build's std::thread and its joins are gone. `DatabasePool::build_names` claims the build with
   one atomic and posts a detached task to the control loop that awaits one `merge` chore of its own IndexChores;
   `set_control(nullptr)` and `finish()` request its stop, so the shutdown's drain ends it. The 6 lock sites left are
   classified in harness/async_allowlist.tsv (pool workers only, in-memory, bounded). Local async_check: 0
   unclassified.
2. Bench: NAME_INDEX_BENCH_ROOT keeps trees at `~/.scratch/name-index-bench/<N>/`; NAME_INDEX_BENCH_MAKE=1 only
   makes them (::mkdir, light job); the strict job times them.

Jobs at 291ec89: build-test 261117-110158-de9685; e2e 261117-110158-9cdd8c; targeted 261117-110204-e7835e (A3-A5);
fixture 261117-110254-f71df8 (light); A2 bench 261117-110258-76c693 (strict, after the fixture). Mutants
(--fails-without, after targeted): no-parents 261117-110214-031049, keep-slash 261117-110217-85a114, drop-delta
261117-110221-d6fa49, lose-merge 261117-110224-f3f320 (A3); no-seek 261117-110228-88596e (A2); no-range
261117-110232-ddea0a, no-index 261117-110235-4fbeb9, no-hook 261117-110238-651fe2 (A4); top-build
261117-110242-e44601 (A5).

## Next

1. When green: bench table into docs/design/name-index.md "Costs"; Evidence from `campaign brief`; ready for review.
   Then delete ~/.scratch/name-index-bench.

## Blocked on

Nothing. Lead rulings received (docs to "index names", lazy build stays, one parser, release gating is the
owner's): 1 and 3 done in xapiand:6dcae4a, 2 kept, 4 noted.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/name-index.md` |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- R1 In memory, rebuilt by one walk at every start; no persisted table (design D1). The directory stays the one
  truth, so A5 has nothing to reconcile.
- R2 The build starts at the node's first pattern (lazy), not at boot (lead ruling: a walk at start costs every
  node with millions of indexes). Until it ends, patterns walk.
- R3 The lookup keeps `--max-search-indexes` as its bound, counting names read and seeks as visits (D7); `_all`
  over the cap stays 400. The walk gate does not apply to lookups.
- R4 Resolve adds on open as well as create, so an index copied in while running is listed once opened (D3).
- R5 The build is a detached task of the control loop awaiting one `merge` chore (lead finding 1, phase 7c D13). No
  request awaits it; patterns walk until it ends. With no control loop nothing is posted. Clearing the control stops it.
- R6 The names' 6 lock sites stay, classified `allow`: taken only on pool workers (resolve's open chore, the build
  chore, a lookup's query chore), no I/O or await under them; loops touch only atomics.
