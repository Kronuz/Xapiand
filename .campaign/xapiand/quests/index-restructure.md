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
   definitions of `basic_index<...>::f`. The class body keeps its data members and declarations.
   `index.hh` stays the include every caller names. Bodies, names and statement order are
   unchanged.
2. Subobjects, only where one wins. Where a mechanism's state and invariants stand apart (the turn
   log's cohort state, the checkpoint window, replay), they become a member class basic_index owns,
   each with its own unit tests. Each extraction is judged on maintenance: fewer members a reader
   must hold in mind, and an invariant stated in one place. No size targets.

## Acceptance criteria

- A1 The suite passes at each step's head: the `build-test` check, plus CI on every platform with
  sanitizers ([ci])
- A2 Step 1 changes no code: a split_check mode (or an extension in cpp-libraries' tools, with its
  own fixture) proves every moved member body reappears unchanged as an out-of-class definition. A
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
- It starts the day turn-log-parity lands: six of turn-log-parity's tasks and its fix round edit
  index.hh, and a split under open branches costs them their edits (SPLITTING.md, "When").
  field-check-at-map also holds index.hh.

## May land early

Step 1, on its own proof (A1, A2), before step 2 starts.
