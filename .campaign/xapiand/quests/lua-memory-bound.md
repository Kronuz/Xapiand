# lua-memory-bound: Lua memory has a node-wide bound below host RAM

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status lua-memory-bound`.

## Outcome

A node's Lua memory has a bound below host RAM whatever scripts clients PUT: a call's garbage does not stay in cached states, or the cache is sized in bytes. From x7-gate security S1 (X4-N2): 57 MiB per distinct script, ~100 GiB ceiling at the default 100 states per thread; blocks a release to untrusted clients.

## Acceptance criteria

- A1 `lua_mem_seq.py` (security review probe) levels off at the defaults: RSS stays under a stated bound over 200 distinct scripts: a job before and after
- A2 One script-cache default: `opts.h` and the CLI agree, and the default is sized from the measured bound: a test
- A3 T10's Lua rows still pass (flood and per-call caps): the rows job
- A4 The suite passes and CI is green at the landed code

## Contracts

<What this quest provides or consumes (APIs, formats, protocols), stated precisely enough that a
quest depending on it can start before it lands.>

## Decisions

<Decisions made, and by whom; open questions, naming the owner or the lead.>

## May land early

<Internal code behind tests, unless the quest is a format, wire or public API. "None" when
everything waits for the review; `land --upto` refuses then.>
