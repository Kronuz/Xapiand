# stox-no-libc: journal

State: waiting on jobs at strict-stox:aebfcd9 (lead's header-comment commit for A6): 261123-143451-1516fe, 261123-143454-c88998, 261123-143457-db487a, 261123-143459-89e019; then CI there.
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
| A1 | 261123-143451-1516fe (build-test, reruns 261123-130429-df3270) at aebfcd9. Earlier 261123-112824-c866b0 failed at 5637e68 (old-route asserts; see Rulings) |
| A2 | 261123-143454-c88998 (reruns 261123-130432-96375e) at aebfcd9: no libc call for a leading space, '+', through stod/stof; fails-without 261123-130437-f0fa57 (old header, SIGABRT in Apple's strtod path) |
| A3 | 261123-143457-db487a (reruns 261123-130445-dabdaa) at aebfcd9: corpus plus 1,000,000 strings equal to libc |
| A4 | 261123-143459-89e019 (reruns 261123-130449-dfdd7f, passed at 85260bc) at aebfcd9: 15 CPU-minutes of fuzzing clean on macOS |
| A5 | CI at aebfcd9 (85260bc green on every platform, macos-arm64 included) |
| A6 | `README.md`: "elsewhere (x86_64 and"; `strict_stox.hh`: "strto* decides the rest" (lead, aebfcd9: README names the texts that reach libc and stold by platform; the header's top comment lists the same) |

## Result

stod and stof decide decimal text after C-locale white space or a single '+' in the library (`fp_lead`, strict_stox.hh, through the existing `fp_slow`), so Apple's strtod is never asked for it and the 209-byte abort cannot happen there. Hex, inf, nan, '+-', a directed rounding mode and a non-'.' locale still go to libc (fp_slow returns 0 for the last two); stold stays on strtold. Open: the header's top comment (strict_stox.hh:169-174) and README.md:29 still list white space and '+' as libc's for floats; the lead fixes both on main after landing, to keep the tested tree.

## Rulings I made

- CI macos-arm64 (run 45251406255): long double is double there, so strtold aborts. Fixed at the new head: `fp_exact<R>` maps long double to double when LDBL_MANT_DIG == DBL_MANT_DIG; `_fast`, `_lead`, `_invalid_tail` use it, so stold follows stod (value widened); elsewhere strtold. The stold hook is `STRICT_STOX_STRTOLD` (counted under COUNT_LIBC). The local host is x86_64, so the double-format branch is exercised by job 261123-130441-b494ea (`-mlong-double-64`); A5 is CI's. A6: README.md and the header comment updated.

- build-test 261123-112824-c866b0 failed at 5637e68: strict_stox_slow asserted the old route (test_reach_libc: "  1", "+1" reach libc; test_first_byte: white space and '+' reach strto*). Fixed at strict-stox head: both now assert decided-in-library; hex, inf, nan, directed rounding and locales still assert libc. Rerun: 261123-114038-c721f8.
- The 4 `divideMPByMP` assertions in that log are the `libc_safe` children of test/slow_test.cc (a forked child tries libc first, and is expected to abort): the two curated cases at lines 114-115 (the 59-character fixture, each checked for stod and stof). They are printed only when the test fails (ctest hides passing output). My new tests call libc in-process only for strtold and on texts that pass in the replay; not a source of aborts.

- stold left alone (lead's scope message): fuzz/regressions/replay-macos.txt shows Apple's strtold passing on both artifacts
  (platform_libc_strtold_* and platform_strict_stold_* Passed), so stold does not reach the aborting strtod. A2 checks stold on the
  fixtures with every lead equals libc's strtold.
- Still libc (unchanged): hex, inf, nan, rounding modes, non-'.' locales, and locale white space beyond the C set.
