# stox-no-libc: journal

State: waiting on jobs 261123-112824-c866b0 (A1), 261123-112826-d3b415 and 261123-112829-ee098b (A2), 261123-112831-f31ce1 (A3), 261123-112834-f40ac5 (A4)
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
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- stold left alone (lead's scope message): fuzz/regressions/replay-macos.txt shows Apple's strtold passing on both artifacts
  (platform_libc_strtold_* and platform_strict_stold_* Passed), so stold does not reach the aborting strtod. A2 checks stold on the
  fixtures with every lead equals libc's strtold.
- Still libc (unchanged): hex, inf, nan, rounding modes, non-'.' locales, and locale white space beyond the C set.
