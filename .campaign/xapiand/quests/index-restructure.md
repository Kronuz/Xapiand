# index-restructure: strata's index.hh: basic_index's 5,309-line class body split by mechanism

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status index-restructure`.

## Outcome

strata's `include/strata/core/index.hh` is 6,139 lines on main (strata:cc30217f; 106 commits), and
5,309 of them are one class body, `class basic_index` (lines 823-6131), with every member function
defined in the class. SPLITTING.md forbids cutting a class body, so a moves-only split reaches only
the other ~830 lines (`log_state`, `basic_index_options`, `hash_slots`, `io_spill`, `file_sink`,
`lent_frame`). The class holds several mechanisms, each with its own state and invariants: open and
replay, the turn log and group commit, checkpoints, merges, spills and reads. It is not one coherent
unit. This quest gives each mechanism its own header, a maintainer can open one to work on one
mechanism, and no behavior changes.

Two steps, each landable alone:

1. Out-of-class definitions. basic_index's member functions move out of the class body into one
   header per mechanism (`index_open.hh`, `index_turns.hh`, `index_checkpoint.hh`,
   `index_merge.hh`, `index_read.hh`, or the cut its reading shows), as `template <...> inline`
   definitions of `basic_index<...>::f`, under `include/strata/core/index/` or as `index_*.hh`
  beside it. The class body keeps its data members and declarations.
   `index.hh` stays the include every caller names. Bodies, names and statement order are
   unchanged.
2. Subobjects, only where one wins. Where a mechanism's state and invariants stand apart (the turn
   log's cohort state, the checkpoint window, replay), they become a member class basic_index owns,
   each with its own unit tests. Each extraction is judged on maintenance: fewer members a reader
   must hold in mind, and an invariant stated in one place. No size targets.

## Acceptance criteria

- A1 The suite passes at each step's head: the `build-test` check, plus CI on every platform with
  sanitizers ([ci])
- A2 Step 1 changes no code: the `members` check (split_check's member mode, from quest
  split-check-members) passes, and the journal answers each warning. A
  Release+LTO `tools/fnsizes.sh` diff of strata's test binaries, base against step 1, shows no
  function whose size changed, or each difference explained in the journal
- A3 Step 2: each extracted subobject has unit tests of its own invariant, plus a mutant per
  invariant that a named test kills (the log shows a complete build, then the named test failing)
- A4 [artifact] `docs/source-layout.md` lists each new header and what it holds, and
  `docs/mechanisms/index-layout.md` says which mechanism lives where, and why each one stays
  inside basic_index or leaves it

## Contracts

`strata:index`'s public API stays the same. Callers include `strata/core/index.hh` and see the same
names.

## Decisions

- Lead, 2026-11-21 (owner asked why index.hh is still this big): turn-log-parity's planned `split`
  task never started; its trigger (after group-commit and large-tx merge, before tolerant-open) passed
  when tolerant-open merged first. It would have reached only the ~830 lines outside the class, so
  this quest replaces it. It is high risk because it moves durability code; step 1 is proved
  mechanically, so one correctness review covers both steps.
- Owner, 2026-11-21 evening: "One class body of 5K+ lines is just not right! Not right at all. It
  makes this single file one of the busiest ones and it also makes the file very very hard to
  understand." So it starts now, on main, instead of after turn-log-parity.
- Lead, 2026-11-21 (step 1 at strata:88058f27, its R3 and R4): the moved definitions of this class
  template stay without `inline` (in-class definitions carry no inlining hint, so codegen keeps);
  the 21 trailing return types are a shape split-check-members' mode accepts. The mover stays
  committed under `.scratch/index-restructure/` until split-check-members graduates it to
  cpp-libraries' `tools/outline.py`; then it leaves this branch.
- Lead, 2026-11-21: turn-log-parity is the only open branch editing index.hh (201 lines, plus its
  tasks). It lands first, unchanged. Then this quest merges main and re-applies those edits inside
  the moved definitions, and the `members` check reruns against the new main, so the proof always
  covers a pure move. Starting under turn-log-parity's file set is recorded with --force on start.
  field-check-at-map (planned) builds on the new layout.

## May land early

Step 1, on its own proof (A1, A2), before step 2 starts.
