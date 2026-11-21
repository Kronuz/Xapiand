# split-datetime: journal

State: waiting on jobs 261121-122646-ec8c05 (bench base) 261121-123009-72a6f6 (bench base A/A) 261121-122640-21e252 (bench split, at c6d6ff7); older jobs at c6d6ff7 are superseded by the next push
Quest: .campaign/xapiand/quests/split-datetime.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

datetime.cc split (datetime-wt c6d6ff7) into datetime.cc (parse), datetime_math.cc, datetime_calendar.cc, datetime_format.cc, datetime_time.cc + internal datetime_detail.h. Perf risk: isValidDate/getDays_month/timegm are now cross-TU calls from the hot parse and math paths; bench jobs at base (ec8c05) and head (21e252) decide whether calendar rejoins datetime.cc.

## Next

1. Read bench base, base A/A and head: if a hot row regresses beyond the A/A band, move datetime_calendar.cc back into datetime.cc (and its row in source-layout.md). Then push once and resubmit A1, A6, A2, A3-head and the benches at the pushed head (359745b or later). Old: read bench base vs head (old/new ratio per row); if a hot row regresses, move calendar back into datetime.cc and rerun.
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
- Shared helpers in datetime_detail.h are `inline` (lead's instruction), at global scope, no namespace (a namespace would rename call sites). Dropped `static` on each, so split_check prints a warning for: floor_div, days_from_civil, days_from_civil_pos, civil_from_days, civil_from_secs, set_from_secs, secs_of, apply_tz, parse_fsec_long, parse_fsec, parse_dotted_fsec (functions) and MIN_YEAR1_SECS, MAX_FRACTION_DIGITS, MAX_TERM_CHARS (constants, now `inline constexpr`). Reason: one copy and one address of each, no per-TU copies, no -Wunused-function. `datetime.cc` keeps its own `static` helpers.
- AGENTS.md/ARCHITECTURE.md: one line each fixed (repo map line for datetime.cc, 'single TU' line).
