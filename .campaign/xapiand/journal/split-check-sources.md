# split-check-sources: journal

State: ready to land (A3: CI passed at cpp-libraries:d1bca40; the main merge changed only cmake.yml, which tools.yml's path filter skips)
Quest: .campaign/xapiand/quests/split-check-sources.md (the lead's; never edit it)

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
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261121-124545-c9ce67 (the test check: test_layout_note_and_cmake_lines_are_not_code, test_line_moved_into_an_inc_fragment_is_lost) |
| A2 | 261121-124545-96b4e5 (lint: ruff check and format), 261121-124545-c9ce67 (test: 44 pass); both reused at 7fb030488886, whose tools/ equals d1bca4032097 |
| A3 | CI green at d1bca4032097 (cpp-libraries tools.yml); 7fb030488886 only merges main's cmake.yml, outside tools.yml's paths, so no run fires |
| A4 | 261121-124545-c9ce67 (test_include_guards_are_allowed, test_a_new_macro_is_not_a_guard) |
| A5 | 261121-124545-c9ce67 (static-to-inline, added inline, namespace-scope detail using as warnings; using inside a function and `using namespace std;` fail) |

## Result

split_check.py checks C and C++ sources only, allows include guards, and reports a static or added `inline`, a namespace-scope `using` of a detail namespace, and dropped includes or comments as warnings the journal answers (cpp-libraries:3832c0d, f9b4607, d1bca40; docs/SPLITTING.md). The 11 library splits and 13 earlier ones rerun A2 with it.

## Rulings I made

- `using namespace std;` at namespace scope still fails; only a detail namespace is allowed (lead).
