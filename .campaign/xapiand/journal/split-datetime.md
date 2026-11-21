# split-datetime: journal

State: waiting on jobs 261121-122624-6b5417 261121-122624-03379d 261121-122624-e89922 261121-122626-0ba83a 261121-122627-8f659b 261121-122628-2ad214 261121-122640-21e252 261121-122646-ec8c05
Quest: .campaign/xapiand/quests/split-datetime.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

datetime.cc split (datetime-wt c6d6ff7) into datetime.cc (parse), datetime_math.cc, datetime_calendar.cc, datetime_format.cc, datetime_time.cc + internal datetime_detail.h. Perf risk: isValidDate/getDays_month/timegm are now cross-TU calls from the hot parse and math paths; bench jobs at base (ec8c05) and head (21e252) decide whether calendar rejoins datetime.cc.

## Next

1. Read bench base vs head (old/new ratio per row); if a hot row regresses, move calendar back into datetime.cc and rerun.
2. Finish docs/source-layout.md (written, uncommitted), commit, push, fill Evidence.
3. Report: AGENTS.md and ARCHITECTURE.md (outside file set) say 'one .cc / single TU' and need the lead's edit.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

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

- test/diff_test.cc (1,182 lines) kept whole: one seeded stream + recorded digest + shared file-scope counters + class header; reasons in docs/source-layout.md.
- Helpers shared across pieces stay global-scope `static` in datetime_detail.h, not a `detail` namespace (that would rename call sites, a non-move). Consequence: unused static non-inline helpers (parse_fsec_long, parse_dotted_fsec) in pieces that include it; only matters under GCC -Wunused-function, not enabled in CI.
