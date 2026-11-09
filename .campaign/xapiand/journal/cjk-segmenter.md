# cjk-segmenter: journal

State: working
Quest: .campaign/xapiand/quests/cjk-segmenter.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Segmenter written and matching ICU (analysis:9623a02): rules DFA from ICU (cjk/tables.hh), dictionary
DP over a Kronuz/fst (data/cjdict.txt built at build time), ICU oracle in tools/icu_compare.cc.
Local dev runs (not evidence): 3,034 vectors pass, zh/ja/ko corpora 0 differences, dict identical,
sweep 0 differences outside Complex_Context scripts (Lao/Khmer/Myanmar: ICU's own engines).
Now: timing smoke vs ICU, then docs (mechanism note, README), then jobs.

## Next

1. Timing smoke (A5), docs/cjk-segmenter.md (A6), README + LICENSE-ICU naming (A3).
2. Jobs: build-test (`campaign verify`), A2 corpus+sweep+dict job, A4 fuzz 5 min ASan+UBSan, A5 bench.

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <job id at the current code> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
