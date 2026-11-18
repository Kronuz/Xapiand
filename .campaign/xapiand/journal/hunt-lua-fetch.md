# hunt-lua-fetch: journal

State: working
Quest: .campaign/xapiand/quests/hunt-lua-fetch.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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

## State (redirected by lead: git pin)
Waiting on jobs 261117-222425-9f4e00 (tag vs tarball src byte compare, at 16bb966), 261117-223014-66e78d (A1,A2: fetch by commit + drift pass/wrong-pin fail), build 261117-223018-18d73d (A3).
Done at 2ae51d6a8: Lua from lua/lua commit 1ab3208a (GIT_SHALLOW), flat paths, patch re-rooted (sha 9426b7b8...), drift check on commit, tarball URL+hash dropped; cppcodec/CLI11 pinned by commit; FindGTest/ExternalProjectInstall deleted (only comment/notice references; THIRD-PARTY-NOTICES and its generator still name FindGTest.cmake: outside file set).
Mirror hash earlier: lua.org and PUC-Rio mirror both sha256 9fbf5e28... (pin).
Next: read job results, record Evidence, A4/A5 per new quest file.
