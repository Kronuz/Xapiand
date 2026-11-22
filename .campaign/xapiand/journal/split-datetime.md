# split-datetime: journal

State: waiting on jobs 261121-160518-63e0f2 (A1 build-test) 261121-160515-3084ef (A2 split_check, fixed script) 261121-160519-3cbaa9 + 261121-160531-3f29b1 (A3 head, base) 261121-160522-0e7ac1 (A6) and benches 261121-122646-ec8c05 (base) 261121-123009-72a6f6 (base A/A) 261121-160540-27adfc (head ee3d707); all queued
Quest: .campaign/xapiand/quests/split-datetime.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

datetime.cc split (datetime-wt c6d6ff7) into datetime.cc (parse), datetime_math.cc, datetime_calendar.cc, datetime_format.cc, datetime_time.cc + internal datetime_detail.h. Perf risk: isValidDate/getDays_month/timegm are now cross-TU calls from the hot parse and math paths; bench jobs at base (ec8c05) and head (21e252) decide whether calendar rejoins datetime.cc.

## Next

1. When the three benches finish, read base vs A/A band vs head (old/new per row). If a hot row (isValidDate, getDaysInMonth, parse, math) regresses beyond the A/A band, move datetime_calendar.cc back into datetime.cc and drop its row from docs/source-layout.md, push, rerun A1/A2/A3/A6. Put the numbers under Result.
2. When the A-jobs pass, check A2 warnings against Rulings (the inline detail helpers) and fill the Evidence rows with `campaign brief`'s table.
3. Done: docs/source-layout.md committed (5b07a01), AGENTS.md and ARCHITECTURE.md already match the split (3 lines, 359745b), main merged (ee3d707), pushed.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261121-160518-63e0f2 (build-test at ee3d707); replaces 261121-122624-6b5417 (ran c6d6ff7) and 261121-122624-03379d (old split check, failed on the checker's .md and include-guard bugs, not the split) |
| A2 | 261121-160515-3084ef (split_check from split-check-sources, at ee3d707); replaces 261121-122624-e89922 (failed: exit 2, old checker) and 261121-122624-03379d. Gate copy 261121-160518-8a9d65 uses the pre-fix script and is expected to fail the same way |
| A3 | 261121-160519-3cbaa9 (head) vs 261121-160531-3f29b1 (base b5d46a8), diffed; replaces 261121-140751-b76baa (cancelled by the resubmit), 261121-122627-8f659b (failed exit 125) and 261121-122626-0ba83a (c6d6ff7) |
| A4 | docs/source-layout.md (5b07a01): a row per new file, each .cc/.h's first line states what it holds, test/diff_test.cc kept whole with reasons |
| A5 | CI on the pushed branch ee3d707 (not yet read) |
| A6 | 261121-160522-0e7ac1 at ee3d707; replaces 261121-122628-2ad214 (ran c6d6ff7) |
| A7 | the lead's read |

Stale queued jobs at 359745b (261121-155141-64011b, 261121-155608-f3ce5c, 261121-155751-6e43a6): superseded by the ee3d707 ones. The brief's ci.yml out-of-scope warning comes from the main merge (origin/main is ahead of the local main), not from this quest.

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- test/diff_test.cc (1,182 lines) kept whole: one seeded stream + recorded digest + shared file-scope counters + class header; reasons in docs/source-layout.md.
- Shared helpers in datetime_detail.h are `inline` (lead's instruction), at global scope, no namespace (a namespace would rename call sites). Dropped `static` on each, so split_check prints a warning for: floor_div, days_from_civil, days_from_civil_pos, civil_from_days, civil_from_secs, set_from_secs, secs_of, apply_tz, parse_fsec_long, parse_fsec, parse_dotted_fsec (functions) and MIN_YEAR1_SECS, MAX_FRACTION_DIGITS, MAX_TERM_CHARS (constants, now `inline constexpr`). Reason: one copy and one address of each, no per-TU copies, no -Wunused-function. `datetime.cc` keeps its own `static` helpers.
- AGENTS.md/ARCHITECTURE.md: one line each fixed (repo map line for datetime.cc, 'single TU' line).
