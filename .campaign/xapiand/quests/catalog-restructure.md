# catalog-restructure: strata's catalog: the 2,374-line class body by mechanism (pin protocol, slot transitions, eviction policy)

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status catalog-restructure`.

## Outcome

strata's `include/strata/core/catalog.hh` is 2,614 lines on main, and `class catalog` (line 239) is
2,374 of them. It is the second-busiest file in strata: 55 commits in the last 30 days, 10 quests'
file sets, and 3 open ones (group-catalog, recovery-tooling, turn-log-parity). One class holds four
mechanisms with their own invariants:
- the pin protocol: `acquire_impl`, pin and unpin, `reopen`, `pin_awaiter`;
- the slot transitions: the open, wake, demote, close, replace and shutdown chores and `end_at`, one
  state machine over the slot state word;
- the S3-FIFO eviction policy: `policy_pass` and `evict`;
- observation: the reports the tests, the sweep and T4 read.

Step 1: the nested types (`slot`, `host`, `pin`, `pin_awaiter`, `shutdown_awaiter`) stay declared in
the class with their multi-line members moved out (cpp-libraries docs/SPLITTING.md; split_check's
members mode proves only that shape), and member definitions move out of the class into
one header per mechanism (`catalog_pin.hh`, `catalog_transitions.hh`, `catalog_policy.hh`,
`catalog_observe.hh`, or the cut its reading supports). The transitions stay whole in one header,
because they are one state machine. `catalog.hh` stays the include callers name. Moved with
cpp-libraries' `tools/outline.py` and proved with split_check's member mode.

Step 2: the eviction policy becomes a subobject the catalog owns, over slot indices and the policy
limits, with unit tests of its own. A policy change and a transition change then land
independently, which is the parallel-work gain.

## Acceptance criteria

- A1 The suite passes at each step's head: the `build-test` check, and CI on every platform with
  sanitizers ([ci]); TSan on the catalog tests at step 2's head
- A2 [decision] The move changes no code: the `members` check passes, and the journal answers each warning.
  A Release `tools/fnsizes.sh` diff of the test binaries, base against head, in one job, shows no
  function whose size changed, or each difference explained in the journal
- A3 Every new header compiles alone: `tools/selfcontained.sh` over the new headers
- A4 Step 2: the policy's own tests (`test/catalog_policy_test.cc`) check its order and limits by
  value, a mutant per policy invariant is killed by a named test (the log shows a complete build,
  then the named test failing), and the catalog's behavior tests pass unchanged
- A5 [artifact] `docs/mechanisms/catalog-layout.md` says which mechanism lives where and why the
  transitions stay one header, and `docs/source-layout.md` lists each header

## Contracts

`strata:catalog`'s public API stays the same.

## Decisions

- Owner, 2026-11-21, on the class bodies over 1,000 lines: "These files would have to be carefully
  split. I would still split them with craftmanship and aiming at improving maintainability and
  parallel work. Decide which are worth and how to split them cleanly."
- Lead, 2026-11-21: worth it. It is the busiest file after index.hh with several mechanisms in one
  class. It needs group-catalog, which edits catalog.hh now. High risk (concurrency): one
  correctness review, whose questions are a transition's ordering kept across headers, the
  policy's view of slot state at step 2, and pin lifetimes.
- Lead, 2026-11-24 (contributor's R-nested): nested types stay declared in the class, as in
  index-restructure (its R9) and SPLITTING.md; their multi-line members move out. docs/mechanisms/
  catalog.md joins the file set for the `policy_mu_` rename (R6).

## May land early

Step 1 on its own proof, before step 2 starts.
