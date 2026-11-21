# split-check: journal

State: waiting on jobs 261121-115533-bc4474 (A1 tests), 261121-115537-c54230 (A2 lint) at cpp-libraries 64b3d5b35dfd
Quest: .campaign/xapiand/quests/split-check.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Decisions

- Lead, 2026-11-21 (msg to contributor): (1) namespace-scope function declarations and `extern` variable declarations are allowed added shapes, each listed as a warning next to the `static` ones; (2) a reorder inside a function must fail: moved runs must start and end at a top-level boundary of their base file (namespace braces not counted; strings, chars and comments skipped), whole declarations may reorder; README drops the `git diff --color-moved` advice; (3) size limit on C/C++ files only and the default base order are fine.

## Doing

Lead rulings 1-3 implemented at cpp-libraries 64b3d5b35dfd (pushed). First version (7e87bc5) was green
on both jobs (261121-115038-6de98e tests, 261121-115040-dc266d lint); earlier failures at 8bcb082
(261121-114542-8bc525, 261121-114543-7b5b1b) were fixture line numbers and lint, fixed.

## Next

1. Read the two jobs' logs; fix failures; rerun until green at the head commit.
2. Fill the Evidence table from `campaign brief`, then the Result.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-115533-bc4474 (tests at 64b3d5b35dfd, pending) |
| A2 | job 261121-115537-c54230 (ruff check, ruff format --check, pyrefly at 64b3d5b35dfd; pending) |
| A3 | [artifact] cpp-libraries tools/README.md: names split_check.py and `split_check.py [--base REF] [--max-lines N] PATH...` |
| A4 | CI after push (pending) |

## Result

Tool in cpp-libraries tools/split_check.py, documented in tools/README.md. Checks: removed lines
reappear; other added lines are allowed shapes (incl. function/extern declarations, warned); moved
runs start and end at top-level boundaries of their base file (reorder inside a function fails);
size limit; warnings for static, declarations, new anonymous namespaces, moved dynamic initializers.
The order check runs only when the content checks pass (otherwise one changed line also yields
boundary noise). A run edited in place continues through the unchanged lines around it.
Known limit: members inside a class body count as inside it, so moving or reordering them fails.

## Rulings I made

- Default base: first existing of main, master, origin/main, origin/master (quest gave none).
- Renames disabled in the diff: a moved file counts as removed plus added lines.
- File-size limit applies to tracked C/C++ files (.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .inl .tpp)
  under PATHs at HEAD, reported at line N+1.
- A namespace-closing brace is found by parsing the whole file (comments, strings, raw strings), so
  a bare `}` closing a namespace is allowed on either side without a `// namespace` comment.
- `static` is also warned about when added (same as dropped, opposite direction).
- Tool errors (no base, PATH outside the repository) exit 1 with a message; argparse usage errors 2.
