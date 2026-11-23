# split-datetime: journal

State: waiting on jobs 261122-211644-7ddf11 (A1) 261122-211645-adc848 (A2 split) 261122-211653-463c36 (A3 head) 261122-211657-6b0995 (A6) 261122-211701-014155 (bench head) 261122-211735-a95936 261122-211738-8813bf (function sizes head, base)
Quest: .campaign/xapiand/quests/split-datetime.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head datetime-wt 2afe156: the fraction parsers (parse_fsec_long, parse_fsec: datetime.cc only; parse_dotted_fsec: datetime_time.cc only) are back as `static` in the piece that uses them, original lines byte for byte. This clears split check 261121-160518-8a9d65's 2 errors: `[[gnu::noinline]] static` -> `[[gnu::noinline]] inline` is no allowed shape (the checker's static/inline strip doesn't see past the attribute; fixed in the split, not the checker). Header drops <string>, <string_view>, strict_stox.hh (only those parsers used them); datetime.cc regains its base <string> and strict_stox.hh lines.

## Next

1. Read 261122-211645-adc848: expect 0 errors, 11 linkage warnings (the fsec ones gone) plus the <exception> removal; answer them under Rulings.
2. Read bench 261122-211701-014155 against base 261121-122646-ec8c05 and A/A 261121-123009-72a6f6 (use new/old per row, the old column controls drift); diff function sizes 261122-211738-8813bf (base) vs 261122-211735-a95936 (head) for parse/iso8601_scan/timegm/apply_tz.
3. Fill Evidence from `campaign brief`, read CI at 2afe156 (A5).

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
| A5 | CI at 2afe156 (pushed, not yet read) |
| A6 | 261121-160522-0e7ac1 at ee3d707; replaces 261121-122628-2ad214 (ran c6d6ff7) |
| A7 | the lead's read |

Failed 261121-160524-015512 (exit 127, `build/datetime_bench: No such file`): submitted without `--build`, resubmitted 16 s later with it as 261121-160540-27adfc; not a code failure. Bench at ee3d707 (27adfc) vs base (ec8c05, A/A 72a6f6), new/old speedup per row: within the A/A pair except `...+hh:mm` 5.0x (base 5.2x/5.3x) and `.ffffff+hh:mm` 4.8x (4.9x/5.0x), about 4% slower, and `2020/01/02` 104x (91x/94x), faster; the unchanged old column drifted +1-3% that run. The +hh:mm rows go through apply_tz (header inline, called from datetime.cc and datetime_math.cc): the size jobs show whether its inlining changed. ee3d707 superseded by 2afe156; its A1/A2/A3/A6 jobs stay as history.\n\nStale queued jobs at 359745b (261121-155141-64011b, 261121-155608-f3ce5c, 261121-155751-6e43a6): superseded by the ee3d707 ones. The brief's ci.yml out-of-scope warning comes from the main merge (origin/main is ahead of the local main), not from this quest.

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- test/diff_test.cc (1,182 lines) kept whole: one seeded stream + recorded digest + shared file-scope counters + class header; reasons in docs/source-layout.md.
- Shared helpers in datetime_detail.h are `inline` (lead's instruction), at global scope, no namespace (a namespace would rename call sites). Dropped `static` on each, so split_check prints a warning for: floor_div, days_from_civil, days_from_civil_pos, civil_from_days, civil_from_secs, set_from_secs, secs_of, apply_tz (functions) and MIN_YEAR1_SECS, MAX_FRACTION_DIGITS, MAX_TERM_CHARS (constants, now `inline constexpr`). Reason: one copy and one address of each, no per-TU copies, no -Wunused-function. `datetime.cc` keeps its own `static` helpers.
- Fraction parsers stay `static` in their single user (datetime.cc, datetime_time.cc), unchanged lines: a helper one piece uses is not shared, and `[[gnu::noinline]] inline` was a non-move (2afe156).
- AGENTS.md/ARCHITECTURE.md: one line each fixed (repo map line for datetime.cc, 'single TU' line).
