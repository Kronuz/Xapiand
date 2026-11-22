# split-queryparser: journal

State: waiting on jobs 261121-130451-f1eea0 261121-130456-b1dd83
Quest: .campaign/xapiand/quests/split-queryparser.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Split committed and pushed (queryparser:2679882); A2 and A6 passed; build-test (A1) and the ctest -N diff (A3) queued; CI dispatched on the branch (run 44950018532: ci.yml has no quest/** push trigger).

## Next

1. Read f1eea0 and b1dd83 and CI run 44950018532, paste `campaign brief`'s Evidence table, push the branch, set "ready for review".

## Blocked on

The lead (msg-212): queryparser's local `main` (40743e3) is stale, patch-equivalent to an old
cut of origin/main; origin/main is 64c9a97, 20 commits further. The quest's `split` check
(`--base main`) and the landing checks compare against local main until it is reset
(`git -C /Users/kronuz/code/Kronuz/queryparser branch -f main origin/main`; main is checked out
nowhere). Until then the `split` gate job fails on the 20 unrelated commits; my A2 job runs the
same tool with `--base 64c9a973efdb`.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261121-130451-f1eea0 (pending) |
| A2 | split_check (split-check-sources version, --base 64c9a97) 261121-130453-379519 passed: 0 errors, 0 warnings, 1 note (queryparser.cc 1,295 lines, kept whole: see the layout note); gate `split` 261121-155803-fd79e8 failed: --base main is the stale 40743e3, so it reports the 20 unrelated origin/main commits (10,862 errors, all in files this quest never touched); rerun once main is reset |
| A3 | 261121-130456-b1dd83: configures 64c9a97 and head, diffs `ctest -N` (pending) |
| A4 | `docs/source-layout.md` at queryparser:2679882 |
| A5 | CI run 44950018532 (workflow_dispatch on the quest branch), pending |
| A6 | no header created; job 261121-130500-1fe08d checks no *.h/*.hh/*.hpp/*.hxx was added since 64c9a97 passed: only `docs/source-layout.md` and `queryparser_describe.cc` were added |
| A7 | lead, 2026-11-21: read the split; describe() moved to queryparser_describe.cc, a separate unit with its opening line |

## Result

`query::describe()` and its two helpers (`op_name`, `escape`) moved unchanged from
`queryparser.cc` to `queryparser_describe.cc` (140 lines), added to the `queryparser` target.
`queryparser.cc` keeps the builder, the parser state machine and `parse()` (1,400 -> 1,295 lines):
the `parser` class is about 1,050 lines of one class body and stays whole. Layout note:
`docs/source-layout.md` (new), naming each file and why `queryparser.cc` stays whole.

## Rulings I made

1. Base: the quest started at queryparser's stale local main 40743e3, where `queryparser.cc` has
   603 lines; the quest names 1,361 (09a4ea6, now in origin/main). The empty quest branch was
   rebased onto origin/main 64c9a97 (both stale commits dropped as patch-equivalent; nothing lost).
2. No open branch edits `queryparser.cc`: every local and remote branch is merged into
   origin/main except `origin/wip/laptop/main-2026-11-08`, which only touches
   `.github/workflows/ci.yml`.
3. One cut only: the description renderer is a separate concern (it reads the public tree,
   shares no helper with the parser, and is where the description format every test row compares
   against is changed). The `builder` stays with the parser: its only user, it throws the
   file-local `limit_exceeded` that `parse()` catches; a header for it would add a file and move
   an anonymous-namespace type into it, for no maintenance gain.
4. `#include <cstdio>` moved with `std::snprintf`, its only user; the piece adds `<string>`,
   `<string_view>`, `<vector>` for what it uses.
5. A6 is vacuous: the quest creates no header; the job proves none was added.
