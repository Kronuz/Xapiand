# split-check: journal

State: waiting on jobs 261121-114542-8bc525 (A1 tests), 261121-114543-7b5b1b (A2 lint); both queued behind an exclusive hold
Quest: .campaign/xapiand/quests/split-check.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

tools/split_check.py, tools/test_split_check.py, tools/README.md written and pushed (cpp-libraries
quest/xapiand/split-check cdca25f; jobs ran at 8bcb082, README only after). The tests have never
run: if A1 fails, fix tests or tool, rerun both jobs at the new commit.

## Next

1. Read the two jobs' logs; fix failures; rerun until green at the head commit.
2. `campaign verify split-check --for A1` at the head, fill the Evidence table, Result.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261121-114542-8bc525 (tests, pending) |
| A2 | job 261121-114543-7b5b1b (ruff check, ruff format --check, pyrefly; pending) |
| A3 | [artifact] cpp-libraries tools/README.md: names split_check.py and `split_check.py [--base REF] [--max-lines N] PATH...` |
| A4 | CI after push (pending) |

## Result

Tool in cpp-libraries tools/split_check.py, documented in tools/README.md. Open for the lead:
function-prototype lines added for a previously static function (new header declarations) are not
among the allowed shapes (class/struct/union/enum forward declarations only, as the quest says), so
a split that needs them fails and needs a ruling. Statement reordering inside a function is not
detected (multiset comparison); README says to use `git diff --color-moved`.

## Rulings I made

- Default base: first existing of main, master, origin/main, origin/master (quest gave none).
- Renames disabled in the diff: a moved file counts as removed plus added lines.
- File-size limit applies to tracked C/C++ files (.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .inl .tpp)
  under PATHs at HEAD, reported at line N+1.
- A namespace-closing brace is found by parsing the whole file (comments, strings, raw strings), so
  a bare `}` closing a namespace is allowed on either side without a `// namespace` comment.
- `static` is also warned about when added (same as dropped, opposite direction).
- Tool errors (no base, PATH outside the repository) exit 1 with a message; argparse usage errors 2.
