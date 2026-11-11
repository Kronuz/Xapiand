# cross-shard-followups: journal

State: waiting on jobs 261111-154234-d4410a 261111-154237-c1fcd8
Quest: .campaign/xapiand/quests/cross-shard-followups.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on d4410a and c1fcd8; then State ready for review (paste `campaign brief`'s table if it changes).
(Earlier:) 
The lead's report: the PATCH test failed in the full binary and CI (tests/test_node.cc:2075). Cause (found with a
diag job printing the PATCH's lookups, 261111-140319-4e82d8): not a race. `const MsgPack id{x.id}` with braces
builds a one-element array, so the PATCH looked up the key of `["repatch"]`, found nothing and ended with
doc_not_found at once. Fixed in Xapiand:24c5bacc6 (`MsgPack id(x.id)`); the diagnostics are reverted. The earlier
passing and fails-without jobs (261111-133750-2603c2, -133753-938445, -133756-65f3d0) ran the broken PATCH test, so
the PATCH fails-without meant nothing; all are replaced by these at 24c5bacc6:
- 261111-141746-cf5a35: both tests 50 times each in a loop (A1/A3 pass)
- 261111-141748-7c992a, 261111-141751-586195: fails-without (must_exist disabled by sed), replace_by_term and PATCH
- the suite for A4: 261111-141805-3e0ed7 at 24c5bacc6

## Next

1. When the jobs finish, read results and CI (the old CI run failed at that PATCH test only); paste the Evidence table; State ready for review.
2. Paste `campaign brief`'s Evidence table below, fill Result, set State "ready for review".

## Blocked on

Nothing.

## Evidence

At 24c5bacc6 (the lead: suite 3e0ed7 and the replace_by_term fails-without passed; the PATCH one, 586195, passed the
test with must_exist disabled: the PATCH test does not depend on must_exist, since with it off the write is refused
as another document's key and the PATCH reads again). The part it guards is the version check before doc_not_found
(src/engine/strata/shard.cc:873): without it the stale write is placed afresh over the new document. Its mutant:
261111-154234-d4410a (deletes that line; exit 0 only when the test prints FAIL). A2: 261111-154237-c1fcd8.

| Criterion | Evidence |
|---|---|
| A1 | 261111-141746-cf5a35 (both tests 50 times each); fails without: 261111-141748-7c992a (replace_by_term, must_exist disabled), 261111-154234-d4410a (PATCH, version check removed) |
| A2 | 261111-154237-c1fcd8 (the catch is doc_not_found only); no test can reach a not_found after an applied write (Result) |
| A3 | 261111-141746-cf5a35 (builds with the assert); src/engine/strata/shard.cc `assert(!args.must_exist || !native.key().empty())` |
| A4 | 261111-141805-3e0ed7 |

## Result

N1: the replace_by_term and PATCH tests (tests/test_node.cc) run their tasks on `ParkHost`, a one-thread
engine::Host that reports `idle()`. A task that stops with the host idle is parked on the held turn (its
take_turn armed its deadline and queued its lock in one step of that thread), and the next task starts only then,
so the queue order lookup/read, erase, write is by construction; the test asserts each is not done at its step.
The PATCH test calls `DatabaseHandler::patch` directly (the HTTP layer adds nothing to the race) and keeps the
GET checks. No sleeps order either race. The other test of that quest (stale write, HTTP) is untouched.

N2: `index(did)` catches `errc::doc_not_found` only. A test cannot reach the case: a `not_found` after an applied
write comes from `finish_write`'s commit, and strata raises not_found only on open ("No such database"), which a
shard that took the write has passed; no fault hook exists to make a commit fail with not_found (adding one is a
new mechanism). Reasoning only, as the quest allows.

N3: a must_exist write asserts its document has a key (src/engine/strata/shard.cc); only built with ASSERTS.

N4: stays known: a failed must_exist write after its blob push leaves those blobs unreferenced in the found
shard's storage; the storage's sweep is where it belongs.

## Rulings I made

- N1 mechanism: a test-local host with an idle signal, not a queue-depth counter on ShardEndpoint/async_mutex
  (that needs pool.h and the task library, outside the file set). Idle after a task starts means parked only
  while nothing it awaits resumes from another thread; the tests' later checks (the tasks complete after the
  release, and the end state) would hang or fail if that were wrong.
- Mutants are applied by sed inside the job's own command (the lead's resubmission).
