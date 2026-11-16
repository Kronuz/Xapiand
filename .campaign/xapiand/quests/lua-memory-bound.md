# lua-memory-bound: Lua memory has a node-wide bound below host RAM

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status lua-memory-bound`.

## Outcome

A node's Lua memory has a bound below host RAM whatever scripts clients PUT: a call's garbage does not stay in cached states, or the cache is sized in bytes. From x7-gate security S1 (X4-N2): 57 MiB per distinct script, ~100 GiB ceiling at the default 100 states per thread; blocks a release to untrusted clients.

## Acceptance criteria

- A1 `lua_mem_seq.py` (security review probe) levels off at the defaults: RSS stays under a stated bound over 200 distinct scripts: a job before and after
- A2 One script-cache default: `opts.h` and the CLI agree, and the default is sized from the measured bound: a test
- A3 T10's Lua rows still pass (flood and per-call caps): the rows job
- A4 The memory bound is per call: concurrent calls of one script on one thread (one cached state, src/lua/processor.cc:611, the heap the allocator meters at :144) each get the full `max_bytes`. Today they share it, so T10's 4 library-call loops answer [204, 204, 400, 400] wherever fewer reactors than loops run (platform-rows run 7: arm64 and FreeBSD, Xapiand:36c2c0ede). Proof: a test running two concurrent calls of one script on one reactor that fails on master
- A5 [manual] T10 passes on arm64 and FreeBSD in CI: the platform-rows workflows on the rows branch (master + platform-rows + this quest), checked by the lead from the run ids
- A6 The suite passes at the landed code (the `suite` check; CI is checked by `campaign land`)

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
