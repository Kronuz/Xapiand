# ninja-only-strata: journal

State: waiting on jobs 261121-095327-0995a3 261121-091847-30861e 261121-095312-4789e2
Quest: .campaign/xapiand/quests/ninja-only-strata.md (the lead's; never edit it)

## Doing

Work committed in strata at 9cefc0e5d (guard, 9 files with -G Ninja, AGENTS.md rule, test/ninja_only_check.sh).
Jobs queued at that commit.

## Next

1. When the jobs finish: fill the Evidence rows from `campaign brief`, set State to ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pass job 261121-095327-0995a3 (check at the head); mutant job 261121-091847-30861e (`--fails-without`: the base CMakeLists.txt has no guard, the check must exit nonzero) |
| A2 | suite job 261121-095312-4789e2 (`campaign verify ninja-only-strata`, configured with `-G Ninja`) |
| A3 | `test/ninja_only_check.sh` grep half, run at the head by 261121-095327-0995a3: no configure without `-G Ninja` outside the records |

## Result

Guard in CMakeLists.txt after `project()`: `if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_GENERATOR MATCHES "^Ninja")`
-> `message(FATAL_ERROR "Ninja only: configure with -G Ninja (not ${CMAKE_GENERATOR})")`.
Files changed: AGENTS.md (rule once, in "Verify a change", strata's AGENTS.md has no separate build section),
README.md, bench/catalog_tiers/{CMakeLists.txt,tiers.sh}, bench/hybrid/CMakeLists.txt, bench/idle_index/k6.sh,
bench/open_sync/{build.sh,first_commit_mutant.sh}, docs/design/standalone-turn-log.md, test/vm_repeat.sh.

## Rulings I made

- bench/hybrid/CMakeLists.txt and bench/catalog_tiers/CMakeLists.txt held the configure only in a comment; the
  comment now carries -G Ninja.
- The check's temp tree is under `.scratch/` of the checkout (ignored), removed on exit.
