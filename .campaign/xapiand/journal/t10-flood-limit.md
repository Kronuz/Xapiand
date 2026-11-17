# t10-flood-limit: journal

State: ready for review
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

0. The strict ABBA was contaminated by light holds (fix idle p99 64.9/1.79/73.1 ms, p50 21 ms in F1; master stable 1.22/1.47/1.74; fix sweep
   448daa failed 102.7 ms against 76.7 with a 25.6 ms call, beside the suite's build). Rerun at `exclusive` by /Users/kronuz/.scratch/t10-flood-limit/abba-x.sh:
   M F F M M F M M (rows only; the last two are the A/A pair), then the sweep once per commit (SM master 261117-022543-afc6c2, SF fix
   261117-022547-9ae194). If the fix's idle or beside-light p99 stays above the A/A spread, time the hop's enter-to-run latency at idle.
   Mutant 5e1452, tests 6a526c and the suite d945e6 passed at fbd5d8ab4.

1. When f0fe7e passes: submit the mutant job (`--fails-without`: sed the hop line out of src/lua/processor.cc, build, run test_lua).
2. Read the cost rows: if tiny-write p50/p99 idle or beside light regress, report the numbers before landing.
3. Write the numbers into docs/dev/t10-flood-limit.md (replace the exploration's limit section with the derivation), push.
4. `campaign verify t10-flood-limit` for the suite (A5), rerun test_lua probes (lua-memory sweep) and `lua_bounds.py` full run; set ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | tests on a two-lane scheduler (a call's 41 of 41 resumes after its entry heavy, the request light after; a heavy request stays heavy; a script error and a bound leave it light): 261117-015256-6a526c (test_lua 996 checks, test_loop_yield 846); mutant that stays light (hop removed), `--fails-without`: 261117-015216-5e1452 |
| A2 | exclusive sweep, one reactor, GET p99 at 1/2/4/8/16 clients: master 261117-022543-afc6c2 19.1/20.0/43.4/60.9/67.8 ms (over its 57.2 limit at 8 and 16), fix 261117-022547-9ae194 27.6/26.7/34.7/37.3/38.2 ms (limit 65.7, p50 17 ms flat; GETs at 16 clients 132 vs 275). Cost rows, exclusive ABBA M F F M M F plus an A/A pair: 261117-022514-016422, 79af24, a10581, e8623e, 739b28, ccb986, 0ea9e1, ea75e8; tiny write p99 alone master 1.40 ms (1.27-1.48, 6 runs) vs fix 1.35 (1.31-1.45); beside 8 GET clients master 4.54 vs fix 4.39; inside master's spread |
| A3 | `harness/lua_bounds.py`: T10's flood limit is `(FLOOD_STRETCHES + 1) x` one flood call alone, k = 2, no client or reactor term. [manual] FreeBSD CI's T10 log: the lead |
| A4 | `docs/design/loop-scheduling.md` D7; `docs/dev/t10-flood-limit.md` (derivation, sweep, cost); `docs/mechanisms/lua-bounds.md` |
| A5 | suite 261117-015303-d945e6 (at fbd5d8ab4) |

## Result

A script call runs in the heavy lane (loop-scheduling D7): `Processor::operator()` hops before its first turn and marks the scope light
on exit. At 16 clients on one reactor the light GET's p99 is 38.7 ms against master's 85.7 and its p50 17.0 against 39.1; every client
count is within (2 + 1) x one call alone (66.5 ms). The tiny scripted write's cost is inside master's spread at exclusive (docs/dev/t10-flood-limit.md "Cost"); strict runs beside the suite's build were contaminated. Docs: loop-scheduling.md D7, t10-flood-limit.md, lua-bounds.md. Open: FreeBSD CI's T10 row (lead). Known: `lua_bounds.py --cost --out` is overwritten by the main report dump (logs carry the rows).

## Rulings I made

- The strict ABBA and the failed strict fix sweep (448daa) ran beside the suite's build and the document-gate's; they are not evidence. The exclusive reruns are.

- The hop sits in `Processor::operator()`, not in `DatabaseHandler::call_script` (src/database/handler.cc is outside the file set); it
  precedes the state lock, so the call holds nothing while it waits for its turn. Scripts already yield at every hook, so no new
  yield point can occur under a document lock or a transaction: the one new suspension is the hop at the entry, before anything is held.
- The lane is restored with a scope guard (`enter_light`) and no hop back, as `SearchService::handle`'s guard does (D2 f).
- k = 2: the stretch in progress plus one heavy turn at each of a GET's two hops through the loop (its read, its database chore);
  limit (k + 1) x one flood call alone.

<Decisions inside the quest's scope, so the reviewer can check them.>
