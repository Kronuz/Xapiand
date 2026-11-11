# x7-catalog-timer: journal

State: waiting on jobs 261110-172208-8645ec
Quest: .campaign/xapiand/quests/x7-catalog-timer.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

The async-check fix is at Xapiand:df71eb5fe. build-test 261110-143950-d97a09 at e0b3cd67e FAILED on the async check
(pool.cc's `.` mutex row covered 10 hits, allowed 5); its tests were not the cause. The Asio timers are now in a map
only the loop's thread touches. The callbacks stay under `pending_mtx`, which any thread arms and close() empties
from the destructor's thread, so that mutex cannot go: it has its own row (3 hits). `weak.lock()` is a false positive
with a row of its own. A local `async_check.py --strict --owed` run exits 0.

## Next

1. Read the four jobs; fix what fails; then State "ready for review".

## Blocked on

Nothing (the lead widened the file set to the four test files).

Answered: the lead's D58 message (no msg id): merged master (Xapiand:5b57ca685), implemented (a) as below.
Answered: the lead's async-check message: two of the five sections went, the rest have their own rows (above).
Answered: the lead's file-set and Stop 1 message: the set now holds the four test files. test_node's
`test_the_stop_runs_in_its_stated_order` runs in build-test 261110-143950-d97a09. options.md "Stopping the node", step
2.2 ("The autocommits that were waiting are dropped") still reads true: the autocommits still waiting when Stop 2
begins are dropped. Those that come due during step 2.1 now run during it, which the docs neither claim nor deny.

## Evidence

| Criterion | Evidence | Fails without the fix |
|---|---|---|
| A1 | 261110-172208-8645ec, t1_census at the head (pending). Copies `harness/gate7c_rows.sh` and `harness/stall_load.py` from x7-gate's e7e8e03c5 (`--ref GATE`), since gate7c_rows.sh is not on master. Skipped, never ran: 261110-151943-132d5b and its rerun ef3fd5 (`--after` 261110-143952-aef5a1, which failed at the old code e0b3cd67e) | 261110-143641-8f8cf6 at master 66b310e60 failed: census {unnamed 16, W 16, FS 9, WL 8, main 1, DB-TIMER 1, LOG 1}, total 52, "FAIL ['offender Xapiand:DB-TIMER']" |
| A2 | 261110-151925-5e77bc, build-test at df71eb5fe, passed (test_pool's sweep, TTL and erase cases) | 261110-151928-4ed547, mutant `refs/mutants/x7-catalog-timer/no-arm` (6688a8ada: `after()` returns before arming), passed: `xapiand_test_pool sweep` fails. Earlier: mutant 261110-143953-02e8cb at e0b3cd67e killed (8 of 117 checks fail); build-test 261110-143950-d97a09 at e0b3cd67e failed on the async check |
| A3 | `docs/mechanisms/index-registry-on-catalog.md` item 10: "The timer the catalog needs runs on the control loop's timers: each callback is an Asio timer armed on the loop's thread with `engine::Host::start_timer`" | (artifact) |

## Result

`DatabasePool::Timer` (src/database/pool.cc) arms each catalog callback as an Asio timer on the control loop via
`Host::start_timer`, posted to the loop's raw executor (which runs after the task scheduler closes); it keeps each
callback until it fires, and `close()` (in `~DatabasePool`, after the shutdown, before the catalog) runs the rest at
once. The DB-TIMER thread and the scheduler include are gone. Manager: `_rx` is made before the pool and outlives it;
`run_control()` runs the stopped loop on the main thread, used by `drain_control`, the 50 ms poll of Stop 1's wait and
each 10 ms slice of Stop 3's wait. Note: docs/mechanisms/index-registry-on-catalog.md item 10.

## Rulings I made

0. The Asio timer handles are touched only on the loop's thread; `pending_mtx` guards only the callback map.
1. One Asio timer per catalog callback, not a queue under one timer: the catalog arms few (1 Hz sweep, busy-lease
   reposts), and the pool keeps the callbacks so close() can run them.
2. Arming hops to the loop's thread (AsioHost arms and cancels there only), with `asio::post` to the raw executor
   rather than `post_call`, which drops calls once the loop's scheduler closes before Stop 3.
3. After close(), a timer runs its callback at once (the catalog's destructor may arm while it waits; a callback only
   posts).
4. Stop 1 runs the control loop at its 50 ms poll, so the autocommits that come due then run, where before they waited
   for Stop 2. Stop 3's slice drops from 500 ms to 10 ms (timer latency there).
5. Pool reset order: the pool is reset at manager.cc's end of join(), long after the loop stopped running (break_loop,
   before join) but before `_rx` is destroyed; the loop "running" through the stop is the main thread's pumping above.
