# stox-no-libc: journal

State: ready to land (lead, 2026-11-23): all criteria met at strict-stox:f51037b; CI pending.
Quest: .campaign/xapiand/quests/stox-no-libc.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Branch pushed at strict-stox:5637e68. Change: `fp_lead` in strict_stox.hh routes float/double text with C-locale white space or
a '+' before a decimal number through fp_slow (called from _slow_nothrow/_slow_throw before libc). Tests: `test_lead_fixtures` in
test/slow_test.cc (209-byte and 59-byte fixtures x 5 leads, stod/stof/stold, counting libc), test/lead_diff_test.cc (A3).

## Next

1. When the jobs finish, paste `campaign brief`'s table into Evidence, write Result, set State ready for review.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261123-114038-c721f8 (build-test, Debug, asserts on) passed at f51037b. Earlier 261123-112824-c866b0 failed at 5637e68 (old-route asserts; see Rulings) |
| A2 | 261123-114040-c8d496 passed at f51037b (no libc call for the artifact with a leading space, '+', and through stod/stof); fails-without 261123-114043-114edd exits 134 with the old header (SIGABRT in Apple's strtod) |
| A3 | 261123-120631-8d50a2 (rerun of 261123-112831-f31ce1) passed at f51037b: corpus plus 1,000,000 strings equal to libc |
| A4 | 261123-120631-0ae489 (rerun of 261123-112834-f40ac5) passed at f51037b: 15 CPU-minutes of fuzzing clean on macOS |

## Result

stod and stof decide decimal text after C-locale white space or a single '+' in the library (`fp_lead`, strict_stox.hh, through the existing `fp_slow`), so Apple's strtod is never asked for it and the 209-byte abort cannot happen there. Hex, inf, nan, '+-', a directed rounding mode and a non-'.' locale still go to libc (fp_slow returns 0 for the last two); stold stays on strtold. Open: the header's top comment (strict_stox.hh:169-174) and README.md:29 still list white space and '+' as libc's for floats; the lead fixes both on main after landing, to keep the tested tree.

## Rulings I made

- build-test 261123-112824-c866b0 failed at 5637e68: strict_stox_slow asserted the old route (test_reach_libc: "  1", "+1" reach libc; test_first_byte: white space and '+' reach strto*). Fixed at strict-stox head: both now assert decided-in-library; hex, inf, nan, directed rounding and locales still assert libc. Rerun: 261123-114038-c721f8.
- The 4 `divideMPByMP` assertions in that log are the `libc_safe` children of test/slow_test.cc (a forked child tries libc first, and is expected to abort): the two curated cases at lines 114-115 (the 59-character fixture, each checked for stod and stof). They are printed only when the test fails (ctest hides passing output). My new tests call libc in-process only for strtold and on texts that pass in the replay; not a source of aborts.

- stold left alone (lead's scope message): fuzz/regressions/replay-macos.txt shows Apple's strtold passing on both artifacts
  (platform_libc_strtold_* and platform_strict_stold_* Passed), so stold does not reach the aborting strtod. A2 checks stold on the
  fixtures with every lead equals libc's strtold.
- Still libc (unchanged): hex, inf, nan, rounding modes, non-'.' locales, and locale white space beyond the C set.
