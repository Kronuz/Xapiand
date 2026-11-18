# cheap-reopen: journal

State: working
Quest: .campaign/xapiand/quests/cheap-reopen.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Measuring the reopen at main (4adce97ce). Jobs: build 261118-053909-2e29fb (`.scratch/build.sh build-perf`,
RelWithDebInfo, LTO off, frame pointers), create 261118-053916-954b10 (2,500 indexes in the slot's `wide2500`),
A2 before 261118-053917-32ddb3 (`.scratch/gate.sh`, pool 200/5000 ABAB, 10 searches each), profile
261118-053918-1af4e1 (`.scratch/profile.sh`, `sample` 12 s over pool-200 searches; call graph in its sample.txt).

## Next

1. Attribute the reopen from the profile: strata open (scan, per-file open+size, manifest read, eager maps), the
   Xapiand payload (ShardEndpoint, metadata read, WAL check), the close, and the search proper.
2. Pick the mechanism and write docs/design/cheap-reopen.md.

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
