# faultlab-journal-oracle: journal

State: waiting on jobs 261121-034036-778aff (A1 regenerate-and-diff), 261121-034118-d9974d (A2 as amended: default configure, grep without test/fixtures; lead), 261121-034039-bc66b3 (suite); A3 CI.
Quest: .campaign/xapiand/quests/faultlab-journal-oracle.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

faultlab:e3fcfe3 pushed: fixture and test/fixtures/regenerate_memory_io.sh committed. Waiting on the three jobs above.

## Next

1. When the jobs pass: State "ready for review"; A3 is CI on the pushed branch.
2. A2 is expected to FAIL on the quest's grep: the generator's `#include "journal/memory_io.h"` (test/fixtures/memory_io_gen.cc:9) matches `journal/memory_io`. Default build fetches nothing from journal (CMake only with FAULTLAB_REGENERATE_MEMORY_IO=ON).

## Blocked on

Lead: A2's grep conflicts with the generator the quest asks for. Options: (a) narrow the grep to `':!docs/reviews' ':!test/fixtures'` (recommended); (b) pass the header path to the generator as a CMake define (dodges the literal check, not recommended). Everything else in the grep is already renamed.

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
