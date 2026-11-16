# lua-memory-bound review: correctness

Commit: Xapiand:fa306169f7bcab68ee18b87f20b789d9b4eb2585 (quest/xapiand/lua-memory-bound; its last code commit is
2aa1a1ca5, then a merge from master and journal commits)
Verdict: APPROVE
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- test_lua 261116-132958-07dd8f at ff85dcd34 (971 checks) with the round-5 mutant 261116-122927-714fb5 (7 FAILs,
  at least one in each of the S1-S4 tests) and the A4 mutant 261116-101214-ce0a1e: the new tests fail without the
  change. fa306169f differs from ff85dcd34 only by a master merge outside `src/lua` and journal commits.
- Suite 261116-133001-b680b7, TSan test_lua + fuzz_lua 261116-124104-b53087, ASan lifetimes 261116-133011-1a8295.
- CI on the rows branch dc8988e42 (ff85dcd34 + platform-rows): arm64 43960602513 and FreeBSD 43960602117 green,
  `gate7c_rows: PASS`. Its control without this quest (86eeb0e76): FreeBSD 43960604601 fails with the library-call
  loops at `[400, 204, 400, 204]` and `[400, 204, 204, 400]` (A4's defect, fixed here), and its flood rows answer
  p99 63.58 ms and 122.84 ms: the FreeBSD flood p99 the journal's round 6 left open (110-114 ms) is master's too,
  not introduced by this quest.
- The security review's S1-S4 probes and their reruns (f6a627, 5bec4c, 5553a1): I did not repeat them.

## What I checked myself

Probes are `.scratch/review-lua-memory-bound-correctness/` in the home checkout. `run.sh QUEST` patches
tests/test_lua.cc's TEST_MAIN with `$PROBES` (default `probes.inc`; `patch.sh`), builds `xapiand_test_lua`
(RelWithDebInfo, asserts on), restores the file and runs it; `QUEST=1` at the quest, `QUEST=0` at master
94a9a1b44 (slot `review-lua-corr-master`), where only the API-neutral probes build. Every call in Q1/Q2 checks its
own result: the stored params (`a`), its own params (`b`, and `shared` overriding the stored one), its own doc's
`id`, a 256 KiB string, and the garbage count `n`; a cancelled call must leave its doc as it came.

1. Q1, results through every path (job 261116-143542-1d35b9, `probes.inc` Q1a-Q1d). Sequential calls on the cached
   state that grow by 64 KiB or more (collect), by less (no collect), and after a script error: results right,
   resident 89,796 -> 92,042 B. Four overlapping calls, small and large script: each its own result. A cancel at every
   resume of the target's whole life (wait, run, collection) on each path: cached (24 resumes: 17 cancelled, 10 kept),
   a state of its own beside a holder (48: 34 cancelled, 17 kept), queued behind a holder of a large script (249: 61
   cancelled, 8 kept); 0 bad, the holder always right, resident back each time. A call kept its result when the
   cancel landed in the collection (N3/N4). The cache trimmed the target's own entry by bytes while the call was
   suspended mid-run: the call ended right, the next request compiled anew, the cache 49 MiB of 64.
2. Q2, the wait (Q2a-Q2c). Four calls of a large script: only the first has started after 30 resumes; at each
   finish, at most one more started (never two at once); they finish in arrival order; a queued waiter cancelled
   ends cancelled and never runs; the holder cancelled hands the state to the next (finish places `0123`, `1x23`,
   `12x3`, `x123`). A request cancelled before it asks never queues; a holder cut by the instruction bound hands
   the state on. Random arrivals of a small and a large script with random cancels, 12 seeds, 192 calls: 170 ok, 22
   cancelled, 0 wrong, the scheduler drains, both states back to their settled bytes and free. By reading: the
   holder awaits only `budget->checkpoint()` (run and collect, processor.cc:812 and :747), never a lock,
   so a waiter cannot block it; `async_mutex` hands the lock over FIFO (task 18eb5167 task.hh:1571-1650) and the
   small-script path's `lock(cancel_token{})` that is not ready is destroyed unawaited, so it never queues.
3. Q3, accounting (Q3a, Q3b; probe L). 400 random calls (ok, small, script error, memory cap, instruction cap,
   result too large, long, overlapping pairs with a random cancel): resident stays in 92,430..92,692 B against
   90,446 idle. A thread's cache over 300 random steps (small and 2-8 MiB scripts, a body changed under its name,
   calls that grow or not, overlaps and cancels): never over the budget (peak 65,534 KiB of 65,536), and at the end
   the counted bytes equal the sum of the cached entries' charges (56,493 KiB both). By reading, `lua_alloc`
   changes `used` by the exact delta on every path and `cached_bytes` is recomputed, never carried, so nothing can
   drift. The process heap (probe L, `probes_leak.inc`, jobs 261116-144250-8cc1d8 quest and 261116-144252-c44d86
   master) moves by tens of KiB a call in both builds, up and down, and does not return when the Processor goes:
   not Lua's bytes; master also holds the 64 MiB a memory-capped call leaves (72 MiB after 5 calls), the quest
   does not (15 MiB).
4. Q4, behavior (`review_b_behavior_matrix` at both builds: 1d35b9 and 261116-143544-c10e6d). 24 cases: a doubled
   field, stored and call params merged (the call's win), `_method`, script errors (string, table, after growth),
   instruction, memory (also under pcall) and result-size bounds, globals not kept between calls, a syntax error,
   a replaced and a nil `_doc`, overlapping calls of a small and a large script, two 20 MiB calls at once, a
   cancelled call beside another. Same kind, message and values at master and at the quest. The result's key
   order differs between runs in both builds (Lua's table order), so values were compared, not bytes. One change,
   for the better: a script with 6-8 MiB of constants fails its first call on the memory cap at master (the
   compiler's garbage stays in the state and counts against the call) and runs at the quest, which collects after
   compiling (probe N, `probes_name.inc`, jobs 261116-145910-290206 and master 261116-145912-21ed15; C1's window is `probes_name_window.inc`). Beyond that
   and the documented ones (each overlapping call has the whole cap; a local script's params are only its own
   call's, N2; calls of a script over 16 KiB take turns), I found none, apart from C1.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | no | defect | **A script whose cache charge alone passes the 64 MiB budget runs but is never cached, and a request that then names it with no body silently runs an empty script.** `remember` inserts, then `trim` drops entries oldest first while the bytes pass the budget, down to the new entry itself (processor.cc:596-602, :623). Scripts of 8.25-8.75 MiB of short string constants (charge 65,876-69,374 KiB) run (ok) and are gone after their upload; the name-only request misses, `compile_local` compiles the empty body (`get_name_body` gives "" as the body, script.cc:210-224) and answers ok with the doc unchanged. Each request with the body compiles it again on the reactor (557-577 ms). At master the same scripts stay cached and the name-only request runs them. The empty-body compile on a miss predates this quest (count eviction reaches it too); the byte cap makes it immediate for this window (settled about 56-63 MiB) and sooner for large scripts generally. Not blocking: it narrows behavior only for scripts within a few MiB of the cap. | jobs 261116-150120-f8089e ("8448 KiB upload: ... charge 65876 KiB; ok; cached after: 0 / name only: compiled anew; ok; ran (absent)"), 261116-145910-290206 (5-8 MiB: charge up to 64,127 KiB, cached, name-only runs) and master 261116-145912-21ed15 (cached; name-only runs) | Owner's call. Options: (a) a name-only request that misses the cache answers a ClientError ("send its body") instead of running an empty script: fixes the silent no-op for every eviction, test: probe N's name-only request answers an error; (b) keep the newest entry even past the budget (a thread then holds at most the budget plus one script's charge, under 80 MiB), test: probe N's 8,448 KiB script cached after its upload; (c) keep both as they are and state the window in lua-memory.md. (a) is independent of this quest and could be its own hunt item. |
| C2 | no | gap | Calls of a script over 16 KiB now run one after another on a thread with no bound on the wait but the request's cancel: a call near `max_instructions` holds every other call of that script on its reactor for its whole run, where before they interleaved. The journal's ruling 5 states it; the note's "How" says "waits its turn" but not that the wait is unbounded. | Q2a (finish places `0123`, never two started); processor.cc:886-893 | State it in lua-memory.md's "Limits that remain", or bound the wait (a deadline on `lock`, then a state of its own) if a schema script over 16 KiB is expected to run long. |

## Not run

- Linux and FreeBSD for my probes (macOS x86_64 only); CI's rows cover the node-level rows there.
- Sanitizers on my probes (TSan and ASan runs of the suite are the contributor's, listed above).
- A1's node probe (`lua_mem_seq.py`) at fa306169f: the status lists its jobs as run at older code; rerunning it is
  the lead's (`campaign job rerun 261116-101130-d8dfc8 261116-111156-44ae75`).
- Foreign scripts end to end through `Processor::compile` (needs a node); I checked the stored-params path through
  a Processor built with params, which is what `compile` builds, and the `same_params` comparison by reading.
