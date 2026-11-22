# group-heal: journal

State: waiting on jobs 261121-173201-4aa79d 261121-173206-d6917f 261121-173210-076626 261121-173215-20e4f0 261121-173219-70b405 261121-173223-4bca51 261121-173227-05623f 261121-173231-b9c5e6 261121-173235-40775c 261121-173239-97acf3
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Task `install` merged at strata:2b080579 (task head f05b6672), main merged at strata:c79cf24e
(main's faultlab move: voters_test conflicts resolved, install_test spelled fl::). Both branches
pushed. Quest-head jobs (consensus 8780e44, CMakeLists.txt's pin): plain suite + group_install
261121-173201-4aa79d (A1, A2); install-silent `--fails-without` at mutant strata:31d38956
(refs/mutants/group-heal/install-silent) 261121-173206-d6917f (A2); install_mutants at the head
(A1): skip-file-set 076626, keep-unchecked 20e4f0, write-past-failure 70b405, marker-before-sync
4bca51, holder-unordered 05623f, no-resume b9c5e6, pin-names-unchecked 40775c, serve-past-length
97acf3 (all 261121-1732xx). Task `sim-port` merged.

## Next

1. install's jobs (State): plain must pass, every mutant must fail on its test; fix on the
   quest branch otherwise.
2. repair, witness (order 2, on install's contract, now merged), then sim-rows (order 3).

## Blocked on

Nothing.

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
