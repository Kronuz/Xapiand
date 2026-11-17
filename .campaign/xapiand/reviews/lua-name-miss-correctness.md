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

# Round 2: the F1 fix

Commit: 4e4f647b3506fba6aec36712efac17bcc702b430 (code; the branch head 77b3a750f adds only journals)
Verdict: APPROVE
Date: 2026-11-16

F1 to F4 of round 1 are folded in (F2 docs list, F3 error wording, F4 test asserts the document and the 400). This round checks the schema path by reading the code at this commit; no new probe job (the lead's jobs cover the declare path).

## Evidence I relied on

- 261116-200146-ecb2eb (test_lua with the fix), fails-without 261116-200151-314f45 (master's script.cc: aborts, exit 134) and 261116-200155-3846f1 (without the compile_local refusal); suite 261116-200158-58b4cd at 4e4f647b3506.
- The new test checks values: a cached name-only declaration stores the body (`_lua._body` equals it), an uncached one is a plain ClientError naming the script, and the stored form runs on a fresh thread with an empty cache (`ran == 42`).

## What I checked myself

- `Script::process_script` (script.cc:255-258) keeps the `processor` shared_ptr alive through building `script_data`, so the `string_view` from `Processor::body()` (the source minus the 18-byte prefix, processor.cc:852-856) is valid when the MsgPack copies it. A script given with a body stores that body (same hash as the cached one, or newly compiled), so full declarations and string-only `_script` values store what they did before.
- Read back from disk or replicated: a stored `_script` is read by `feed_script` (schema.cc:7503-7510), which only copies the property and sets `normalized_script = true`; it never calls `process_script`, so the cache plays no part. A stored body compiles on any node and thread; replication moves the stored property bytes, and nothing in the diff touches it.
- Foreign schemas: `process_script`'s foreign branch returns before `compile_local` and the diff does not touch `Processor::compile`'s foreign path; unchanged.
- Existing indexes whose stored schema already holds `_body:""` (declared name-only before this change): the stored property is read by `feed_script` unchanged, and every write calls `call_script` (schema.cc:2911), which reaches `compile_local` with an empty body. It runs only if the thread happens to have that name cached; otherwise it answers 400. Before, the same schema silently did nothing. Nothing rewrites such a schema, and I did not find or run a path that redeclares an existing index's `_script`.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| G1 | no | gap | Indexes whose stored schema already holds `_body:""` get a 400 on writes from any thread that has not cached the name; there is no migration and I did not confirm that a schema can be redeclared with a body. Such a script never did anything useful (empty, or a cached script by luck), so the population is expected to be near empty; it follows the quest's decision, so it is the owner's call. | script.cc:255-258 and schema.cc:7503-7510, 2911 read at 4e4f647b3 | Options: keep (errors never pass silently); or add a one-line note to `docs/mechanisms/lua-memory.md` that such a schema is fixed by declaring its `_script` with a `_body`, after checking that works. |
| G2 | no | nit | CI at 77b3a750f312 shows FreeBSD failed. This round did not look into it; it is the lead's to attribute (the quest changes only Lua and script files). | `campaign status lua-name-miss` | Triage before landing. |

## Not run

- No end-to-end node probe (HTTP declare, restart, replication); the schema path is from reading code, and the stored-form-on-a-fresh-thread case from the lead's test.
- Whether an existing index's `_script` can be redeclared (G1).
