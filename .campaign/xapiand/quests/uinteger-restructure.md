# uinteger-restructure: uinteger_t: the algorithms leave the 5,709-line class body, one header per family

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status uinteger-restructure`.

## Outcome

`uinteger_t.hh` is 6,042 lines on master, and 5,709 of them are one class body, `class uinteger_t`
(line 171), with all 323 member functions (4,631 lines) defined inside it. split-uinteger-t cut it
into `.inc` fragments, which SPLITTING.md forbids, and then kept it whole (uinteger_t:6815126). The
owner, 2026-11-21: "algorithms should be split into multiple files". This overrides the
single-file contract SPLITTING.md protected.

The class body keeps its data members, nested types and member declarations. Each algorithm
family's definitions move out of the class into a self-contained header of its own under
`uinteger_t/`, as `inline` out-of-class definitions. The family map is the contributor's to settle
by reading. A starting point is split-uinteger-t's cut (uinteger_t:6815126^: config, container,
word and limb kernels, bit operations, schoolbook, Karatsuba and Toom multiplication, NTT, schoolbook,
recursive and Newton division, base conversion, free operators), as headers, never fragments.
`uinteger_t.hh` stays the one include users name: it holds the class and includes the families.
Each family header includes what it uses, so it compiles alone. Pieces under about 100 lines join a
neighbor. Bodies, names, statement order and behavior don't change.

Step 2, only where it wins on maintenance: kernels that use no class state (they take `digit*`
spans) may become free functions in a detail namespace, the mpn layer under the class. It is its
own commit, with its own A2-style proof that calls changed only in qualification.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, and CI on every platform ([ci])
- A2 The move changes no code: the `members` check (split_check's member mode, from
  split-check-members) passes, and the journal answers each of its warnings
- A3 Codegen is unchanged: a Release `tools/fnsizes.sh` table of the test and bench binaries, at
  master and at the head (`diff`), shows no function whose size changed, or each difference
  explained in the journal. One job builds both and prints the diff
- A4 Speed is unchanged: `bench/ab.sh` takes a git ref as a whole tree (`uinteger_t.hh` plus
  `uinteger_t/`), and an interleaved A/B, master against the head, plus an A/A control, under
  `quiet exclusive`, shows no operation slower beyond the A/A's spread. Report the honest magnitude
  per operation and limb count in `docs/perf/` (or bench/results/), with the hosts
- A5 Every new header compiles alone: `tools/selfcontained.sh` over `uinteger_t/*.hh`
- A6 [artifact] `docs/source-layout.md` lists each header and what it holds, and drops the "kept
  whole" section. README and ARCHITECTURE.md say the library is `uinteger_t.hh` plus its
  `uinteger_t/` directory, and CHANGELOG names the change for users who copied the single file
- A7 [manual] The lead reads the family map and agrees that each header is one algorithm family a
  maintainer would open alone

## Contracts

The public API (`uinteger_t`'s members, the free operators, `UINTEGER_T_PUBLIC_IMPLEMENTATION`)
stays the same, and users include `uinteger_t.hh` as before. What changes is distribution: the
library is now a header plus a directory. Xapiand puts uinteger_t's source directory on its include
path (Xapiand CMakeLists.txt:949), so it needs no change.

## Decisions

- Owner, 2026-11-21: the algorithms split into multiple files, overriding the single-file contract.
- Lead, 2026-11-21: risk normal (a behavior-preserving restructure of a shared library, proved by
  member-mode moves, function sizes and an A/B), one correctness review. Its questions: a missed or
  doubled definition, an `inline` lost (an ODR violation in a multi-TU user), a header that compiles
  only after another, and the A/B's noise.

## May land early

Nothing: one landing.
