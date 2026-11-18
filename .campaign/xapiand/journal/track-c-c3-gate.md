# track-c-c3-gate: journal

State: waiting on jobs 261118-025944-a6e140 261118-025951-ea8fde 261118-025953-961e8c 261118-025955-8f229e 261118-030028-8b42e8 261118-030029-db899b 261118-030031-7bd707 261118-030042-dd890d 261118-030044-f57019 261118-030045-a0fff5
Quest: .campaign/xapiand/quests/track-c-c3-gate.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs queued at consensus main b7f48b2414d8 (jobs run in the consensus repo with --repo/--commit; scripts
~/.scratch/c3-gate/{body.sh,vm.sh}: body.sh build|rows|mutant|mutant-unit is the same everywhere; vm.sh
boots document-gate's VM (found stopped, stops on exit, refuses a running one), syncs `git archive` of
consensus, runs body.sh). Per platform: build, rows (ctest consensus_consensus = T5 core rows,
consensus_driver = Driver::repair rows, consensus_simulation = T2 + Driver simulation), mutant
(tools/mutants.sh filtered to 'repair below base_index', 400 seeds x 10000 steps), macOS also
mutant-unit (--fails-without: consensus_test built at the mutant).
macOS: build a6e140, rows ea8fde, mutant 961e8c, mutant-unit 8f229e.
FreeBSD (18784): build 8b42e8, rows db899b, mutant 7bd707.
Linux (18782): build dd890d (after hybrid-go-no-go's last VM job 148235), rows f57019, mutant a0fff5.
docs/gates/track-c-c3.md in the strata worktree has the skeleton; fill rows from the job logs.

## Next

1. When jobs finish: read logs (campaign job show/log), fill docs/gates/track-c-c3.md, rerun a failed row once to separate flake from bug.
2. Run `campaign verify track-c-c3-gate --for A1` (strata check suite at the docs commit), commit, mark ready for review.

## Blocked on

Nothing. One finding for the lead: Driver::repair forwards `Repair` to Core, and T5's rows and the mutant live at Core::step(Repair) (consensus_test, simulation_test); Driver-level coverage is one lone-voter row (driver_test `rows`) and the Driver simulation never repairs. See the gate doc, "Where the rows run".

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
