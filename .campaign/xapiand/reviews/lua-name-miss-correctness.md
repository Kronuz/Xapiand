# lua-name-miss review: correctness

Commit: ac3198b60715786b0eb971364a2a13de120cad21 (code; the branch head f1acd901d916 adds only the journal)
Verdict: APPROVE
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding.

## Evidence I relied on

- 261116-191106-ac85b9 (test_lua with the fix) and its fails-without 261116-185852-7c38a4: the new test covers an evicted script and an over-budget one, by value (the error names the script), and a still-cached script runs by name.
- 261116-191113-863775 (suite). Nothing re-run.

## What I checked myself

Probe job 261116-193914-0b7af2 (scripts under `.scratch/review-lua-name-miss-correctness/`: `run.sh` builds `xapiand_test_lua` at the quest head with `probes.inc` in place of TEST_MAIN; output in the job's `probes.txt`). Results:

- Q1, deliberate empty script. P1: `_name` + `_body:""` cold is refused (1); with the same name cached from a real body it is served (0). `Script` treats an empty body as absent everywhere (`body.empty() ? value : body`, script.cc get_name_body), so a deliberate empty body is indistinguishable from name-only and now 4xx. No test or doc sets an empty body on purpose.
- Q1, schema path (the only caller besides `Processor::compile`; handler.cc:473 is the call path, schema.cc:9087/9102 the declaration path). P2: a name-only script declared while cached on the thread stores `{"_type":"script","_lua":{"_name":"p2-s","_body":""}}` (script.cc:255-262 stores `name_body.second`, not the cached body). Read back on another thread (a stored schema after a restart is the same cold cache) it is refused. Declared cold it is refused at declaration (`Script 'p2-never' is not cached`). Before the change both read-back cases compiled an empty script and silently did nothing; they now answer 400 on every write that hits that schema.
- Q1, remote/replicated and restart: `compile_local` has no remote caller; a foreign script goes through `compile` and `remember_foreign`, which never reach the new throw. A schema that stores a body recompiles from it on a cold cache (non-empty body, no throw). Only a name-only stored script is affected (P2).
- Q2. Key is `'L' + name` (processor.cc:556), looked up by `cached()` before the throw, with no `co_await` between lookup and throw, and the cache is `thread_local` (processor.cc:567), trimmed only by its own thread (remember, `operator()`). No count or byte-budget race can interleave. A processor in use is held by `shared_ptr`, so eviction between compile and run does not matter. P4: five refusals add nothing and evict nothing at a count bound of 2. P3: the cache is per thread; a script uploaded on one thread is "not cached" for a name-only request on another (see F2).
- Q3. `ClientError` maps to 400 (http_utils.h:108,132; only `PayloadTooLarge` is 413). The throw is in `compile`, before `Processor::operator()` runs and before the schema's write, so the document is not touched and nothing is indexed. Read from code only; no end-to-end HTTP run.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | A name-only script in a schema is stored with `_body:""` even when the declaring thread had it cached, so after a restart or on another worker thread every write to that index now answers 400 where it used to no-op silently. The change is the quest's stated decision (owner told 2026-11-16), so this is the owner's call, not a blocker. | P2 in 261116-193914-0b7af2; script.cc:255-262; processor.cc:927-932 | Options: keep as is (errors never pass silently); or make `process_script` refuse a name-only script outright so no schema ever persists an empty body (a separate quest). |
| F2 | no | gap | The docs list only eviction and the byte budget as causes of a miss. The cache is per thread (`thread_local`, processor.cc:567) and starts empty after a restart, so "not cached" also means "uploaded on another worker thread" and "after a restart"; a name-only request is only dependable on the thread that compiled the script. | P3 in 261116-193914-0b7af2; lua-memory.md:98-100 | One clause in the docs note: "or was compiled on another thread, or not yet since the restart". |
| F3 | no | nit | A deliberate empty `_body:""` with a name now answers "not cached; send its '_body'", which misdescribes the request. Harmless (an empty script is a no-op); keep, or word the error "has no body and is not cached". | P1 in 261116-193914-0b7af2 | Optional. |
| F4 | no | nit | The test checks the error text but not the document staying unchanged or the HTTP status; both hold by construction (throw before run, ClientError is 400). A1's "leaves the document unchanged" has no executable check. | tests/test_lua.cc:1323-1357 | Optional: a `call` assertion on the document after the refusal. |

## Not run

- No end-to-end HTTP request or index write through a stored schema; status and unchanged document are from reading code.
- CI (FreeBSD, arm64) was still running at the head; platforms are CI's.
- The probe's Script objects each hold a named MsgPack (a first version with temporaries was discarded as undefined behavior; only the second job's output is used).
