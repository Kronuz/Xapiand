# t10-flood-limit: journal

State: waiting on jobs (ABBA 52bbdf ac1d98 4a9b4c a1e5d9 b5fa04 b0633c; mutant 5e1452; reruns 6a526c abdc04 448daa; suite d945e6)
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

Jobs prove criteria by their tags (`--for A1`); `campaign brief` has the generated table at the head.

| Criterion | Evidence |
|---|---|
| A1 | tests (3, two-lane scheduler: 41 of 41 resumes after the call's entry heavy, request light after; heavy request stays heavy; error and bound leave it light) 261117-015256-6a526c (first pass at 37eda4ed0: 261117-013727-5859b4, 996 + 846 checks); mutant that stays light (hop removed) 261117-015216-5e1452 `--fails-without` |
| A2 | sweep at master 261117-015256-abdc04 and at the fix 261117-015257-448daa (first pass: 0581d4, 919aea): GET p99 at 1/2/4/8/16 clients, master 22.8/22.5/47.2/68.3/85.7 ms, fix 26.3/28.2/45.1/36.0/38.7, limit 66.5 ms (3 x 22.2 ms); p50 at 16 clients 39.1 vs 17.0; GETs served 111 vs 285. Cost rows ABBA (M F F M M F): 52bbdf ac1d98 4a9b4c a1e5d9 b5fa04 b0633c |
| A3 | `harness/lua_bounds.py`: T10's flood limit is `(FLOOD_STRETCHES + 1) x` one flood call alone, k = 2; no client or reactor term. [manual] FreeBSD CI's T10 log: the lead |
| A4 | `docs/design/loop-scheduling.md` D7; `docs/dev/t10-flood-limit.md` (derivation, sweep, cost); `docs/mechanisms/lua-bounds.md` |
| A5 | suite 261117-015303-d945e6 |

## Result

A script call runs in the heavy lane (loop-scheduling D7): `Processor::operator()` hops before its first turn and marks the scope light
on exit. At 16 clients on one reactor the light GET's p99 is 38.7 ms against master's 85.7 and its p50 17.0 against 39.1; every client
count is within (2 + 1) x one call alone (66.5 ms). The tiny scripted write's cost is in the ABBA rows (docs/dev/t10-flood-limit.md,
"Cost", to be filled when the chain finishes). Docs: loop-scheduling.md D7, t10-flood-limit.md, lua-bounds.md. Open: FreeBSD CI's T10
row (lead), the A5 suite and the T10 full run of lua_bounds.py.

## Rulings I made

- The hop sits in `Processor::operator()`, not in `DatabaseHandler::call_script` (src/database/handler.cc is outside the file set); it
  precedes the state lock, so the call holds nothing while it waits for its turn. Scripts already yield at every hook, so no new
  yield point can occur under a document lock or a transaction: the one new suspension is the hop at the entry, before anything is held.
- The lane is restored with a scope guard (`enter_light`) and no hop back, as `SearchService::handle`'s guard does (D2 f).
- k = 2: the stretch in progress plus one heavy turn at each of a GET's two hops through the loop (its read, its database chore);
  limit (k + 1) x one flood call alone.

<Decisions inside the quest's scope, so the reviewer can check them.>
