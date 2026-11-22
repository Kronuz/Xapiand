# group-heal--witness: journal

State: working
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A4 (witness). strata:55168358 and the suite line after it: `include/strata/group/witness.hh`, a
`member` with no index, file set or chore (consensus D9 does the protocol: acknowledges the
witness's own ranges, refuses its proposals/reads, hands off its leadership). The voters' harness
moved verbatim to `test/group/world.hh` (voters_test.cc includes it), plus witness nodes, a
founding membership and role-neutral observers on `node`. `test/group/witness_test.cc` (ctest
`group_witness`): `loss` (each member power-cut at seeds 1-3, writes on, rejoins), `carry` (the
witness alone holds what a returning voter lacks: it leads, refuses a proposal and a read, hands
off), `join` (learner installs, promoted to witness beside a leaving voter, voter removed, each
member lost). Not compiled locally: CI on the pushed task branch is the first build.

## Next

1. CI at the task head compiles and runs group_witness; fix what it finds. Then the A4 job
   (`--for A4`) at the task head, and a mutant: the witness's Sink applying a range it is
   handed (or validating nothing) must fail `group_witness`.
2. Ready for the lead's merge into group-heal.

## Blocked on

Nothing for A4. Finding for Detent (consensus), via the lead: a witness can checkpoint only
through `Core::witness_bound`, which `Driver::GroupState` does not expose, so a strata witness
takes no checkpoint and its log grows to consensus's log limits. Recommendation: the Driver
compacts a witness's log itself (it already acknowledges its ranges), or exposes the bound.
Noted in docs/mechanisms/replication-group.md, What it relies on.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

## Rulings I made

- The witness is its own `member` class, not a mode of `group`: it holds none of group's index,
  file set, chore or pins, and a flag would branch every one of them.
- A joining witness validates the install image with nothing staged: refusing it would leave a
  witness joining a compacted log unable to catch up; consensus keeps the image as its log's base.
- Task file set widened by `test/group/install_suite.sh` (quest file set; install merged at
  strata:2b080579): the group suite builds an explicit target list, so `group_witness_test`
  joins it or ctest finds the test not built.
- `--force` at `campaign start`: install's file set still registered though merged.
