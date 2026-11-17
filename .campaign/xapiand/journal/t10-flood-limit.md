# t10-flood-limit: journal

State: waiting on jobs 261117-013727-5859b4 261117-013730-0581d4 261117-013736-919aea 261117-013747-427f04
Quest: .campaign/xapiand/quests/t10-flood-limit.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Option 1 built (lead's ruling): `Processor::operator()` (src/lua/processor.cc) hops to the heavy lane before its first turn and
marks the scope light on every exit if it was light on entry (a heavy request stays heavy). Commits e78e3a766 (code and tests),
harness commit (`lua_bounds.py --cost`, derived limit), 1036ca25a (docs: D7, lua-bounds.md).
A1 restated for the lead: the script call enters heavy and leaves light, with values (the lane of every resume of a 200,000-iteration
call; heavy request stays heavy; error and bound leave light) on a two-lane test scheduler, plus a mutant that stays light.
Jobs (f0fe7e failed on a wrong binary path, eec143/5d02bf were cancelled by me for the same path error; resubmitted as 5859b4 tests, 0581d4 master cost, 919aea fix cost, 427f04 mutant): f0fe7e builds and runs test_lua and test_loop_yield; eec143 (master, strict) and 5d02bf (fix, strict, `--check`) run
`lua_bounds.py --cost` on a Release build: tiny scripted write idle and beside 8 GET-only clients, then the 1/2/4/8/16 sweep against
(2 + 1) x one flood call. Their logs: /Users/kronuz/.scratch/t10-flood-limit/cost-{master,fix}.{log,json}.

## Next

1. When f0fe7e passes: submit the mutant job (`--fails-without`: sed the hop line out of src/lua/processor.cc, build, run test_lua).
2. Read the cost rows: if tiny-write p50/p99 idle or beside light regress, report the numbers before landing.
3. Write the numbers into docs/dev/t10-flood-limit.md (replace the exploration's limit section with the derivation), push.
4. `campaign verify t10-flood-limit` for the suite (A5), rerun test_lua probes (lua-memory sweep) and `lua_bounds.py` full run; set ready for review.

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

- The hop sits in `Processor::operator()`, not in `DatabaseHandler::call_script` (src/database/handler.cc is outside the file set); it
  precedes the state lock, so the call holds nothing while it waits for its turn. Scripts already yield at every hook, so no new
  yield point can occur under a document lock or a transaction: the one new suspension is the hop at the entry, before anything is held.
- The lane is restored with a scope guard (`enter_light`) and no hop back, as `SearchService::handle`'s guard does (D2 f).
- k = 2: the stretch in progress plus one heavy turn at each of a GET's two hops through the loop (its read, its database chore);
  limit (k + 1) x one flood call alone.

<Decisions inside the quest's scope, so the reviewer can check them.>
