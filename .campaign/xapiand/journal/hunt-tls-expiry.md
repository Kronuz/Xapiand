# hunt-tls-expiry: journal

State: blocked on lead: reactor pin lacks seconds_to_expiry
Quest: .campaign/xapiand/quests/hunt-tls-expiry.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

<The current step, precisely enough for a fresh contributor to continue it.>

## Next

1. <step>

## Blocked on

Pinned reactor 886131ba2f (CMakeLists.txt:831) has no seconds_to_expiry (grep of reactor_tls.h at that commit: nothing). Added by 409c4ce, 27 commits after the pin. Only caller: src/tls.cc:73 (tls_setup warning). Repin is the lead's: options: repin to a reactor commit at or after 409c4ce, then I do the swap (empty optional -> 0 for the warning, i.e. same as before).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
