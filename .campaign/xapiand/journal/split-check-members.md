# split-check-members: journal

State: waiting on jobs 261122-212041-81ad6c 261122-212041-782f93
Quest: .campaign/xapiand/quests/split-check-members.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done at cpp-libraries:237350a on quest/xapiand/split-check-members (worktree
`/Users/kronuz/code/Kronuz/cpp-libraries-wt/xapiand/split-check-members`). The `test` (A1, A2,
A3, A5) and `lint` (A6) gate jobs are queued; the new tests have not run yet (local runs are
refused, so every run is a job).

## Next

1. Read test 261122-212041-81ad6c and lint 261122-212041-782f93. A test failure is a fixture or
   parser bug: fix, commit, `campaign verify split-check-members --for A1,A2,A3,A5`, rerun the two
   real-head jobs at the new commit (commands below).
2. Re-tag lint for A6 (`campaign verify --for` tagged both checks A1,A2,A3,A5); CI green on the
   pushed branch for A6's [ci].
3. Ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | test 261122-212041-81ad6c (pending): `OutlineTest`, `MembersTest` |
| A2 | test 261122-212041-81ad6c (pending): `MutatedMoveTest`, one test per mutation |
| A3 | test 261122-212041-81ad6c (pending): every existing test unchanged; `--members` is opt-in |
| A4 | `docs/SPLITTING.md` "Restructuring a large class" and "What `tools/split_check.py` checks"; `tools/README.md` (cpp-libraries:237350a) |
| A5 | test 261122-212041-81ad6c (pending): `OutlineTest` (outline.py output passes `--members`; compiles before and after) |
| A6 | lint 261122-212041-782f93 (pending); pyrefly 0 errors; CI after push |

Real heads (not criteria; the lead needs them before those quests use the check), as
`campaign job submit --quest split-check-members --repo <repo> --commit <head> --level none
--ref TOOL=/Users/kronuz/code/Kronuz/cpp-libraries@<commit> -- bash -c '/usr/local/bin/python3
"$JOB_REF_TOOL/tools/split_check.py" --base <base> --members <paths>'`:

| Head | Job | Result |
|---|---|---|
| strata index-restructure strata:a7448b3, `--base main --members include/strata/core` | 261122-212043-b21a3d | pass: 0 errors, 0 warnings; `strata::basic_index: 196 member definition(s) moved out of the class` |
| uinteger_t uinteger-restructure uinteger_t:5dd518f, `--base master --members uinteger_t.hh uinteger_t` | 261122-212047-a15ac4 | pass: 0 errors, 0 warnings; `uinteger_t: 354 member definition(s) moved out of the class` |

Both at cpp-libraries:237350a. The counts are the quests' own (196 definitions; 354 member
functions), so neither pass is vacuous. The same runs at cpp-libraries:da69339, before the count
line: 261122-211651-3ffe8f, 261122-211655-7221b7, both exit 0. Superseded and cancelled by the
reruns: 261122-211640-00b10f, 261122-211641-ebcca8.

## Result

`split_check.py --members` (cpp-libraries:da69339, 237350a) proves a class restructure, then runs
the existing checks on the rest of the diff:

1. Each in-class member definition that left the class matches one new out-of-class definition
   by owner, name and canonical head; its body from `{` to `}` differs only by one uniform change
   of leading indentation (column-0 preprocessor lines and raw-string lines exact).
2. Head changes allowed: in-class specifiers and default arguments (function and template)
   dropped, `inline` added, enclosing template headers and class qualifier added, member names in
   the return type qualified or a trailing return type. Constructor initializers kept.
3. The class at HEAD equals the base class with each moved definition replaced by its declaration
   (difflib over normalized non-blank lines); only class-level preprocessor runs may be deleted,
   and those must reappear between the same moved definitions at HEAD.
4. A non-template header definition needs `inline`/`constexpr`; a member type (or a class template
   name without arguments) unqualified in a leading return type fails; a move to a source file
   warns. Every failure names `member C::f (base file:line)`. A pass prints the count per class.

`tools/outline.py` is the mover: index-restructure's map format plus line ranges, `#LINE`
preprocessor keys, nested-class keys, `keep-one-line` and include placement. Fixtures:
`tools/testdata/members/store.hh` (class template) and `wide.hh` (non-template), with maps. Tests
in `tools/test_split_check.py`: 7 outline (A5; 2 compile with `c++ -std=c++20 -Wall -Wextra
-Werror` before and after, and each piece alone), 10 member-mode acceptance and refusal (A1), 5
mutations (A2), 1 command line. Docs: `docs/SPLITTING.md`, `tools/README.md`.

## Rulings I made

- `tools/outline.py` is outside the quest's file set, though A5 names it; I created it there. Its
  tests live in `tools/test_split_check.py` (in the set), so the `test` check runs them. Lead:
  add `tools/outline.py` to the file set.
- Allowed beyond A1's list: a member name in the leading return type qualified (`C::T`,
  `typename C<P>::T`), and a class template's own name written with its arguments. uinteger-
  restructure qualifies (`uinteger_t::digit`); both forms name the same entity, and the check
  refuses the unqualified form, which would resolve at namespace scope.
- Default template arguments are dropped like default arguments (uinteger's `template <typename
  T, typename>`): an out-of-class definition can't repeat them.
- A class-level preprocessor line may leave the class only to sit, at HEAD, between the same moved
  definitions as at the base, in the same order (uinteger's `UINTEGER_T_UNROLL` pair).
- Text after a moved definition's closing `}` on its line stays in the class; neither head has any.
- outline.py takes a trailing return type when the return type names a member; for a function
  returning a function pointer it qualifies those names instead (a trailing return would need the
  declarator rewritten). One-line definitions move unless `keep-one-line: yes` (index's choice).
- A friend defined in the class is not a member: both tools leave it in the class.
