# ninja-only: journal

State: waiting on jobs 261121-232251-214a22 (A1, A3) and suite 261121-234535-6419a0 (A2, priority 5) at fa7443d (lead, 23:45)
Quest: .campaign/xapiand/quests/ninja-only.md (the lead's; never edit it)

## Doing

Work committed at 6ffcee88f (guard, 14 files with -G Ninja, AGENTS.md rule, harness/ninja_only_check.sh).
Jobs queued at that commit.

## Next

1. When the jobs finish: fill the Evidence rows from `campaign brief`, set State to ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pass job 261121-232251-214a22 (check at the head e66db4e, level none: one refused configure and a grep); mutant job 261121-091759-59baa5 (`--fails-without`: the base CMakeLists.txt has no guard, the check must exit nonzero) |
| A2 | suite job 261121-234535-6419a0 at fa7443d (the suite configured with `-G Ninja`, priority 5; passed at d431d16 as 261121-222918-29457f) |
| A3 | `harness/ninja_only_check.sh` (its grep half; run at the head by 261121-232251-214a22): lists configures without `-G Ninja` outside the records; docs/dev/engine-census.md is excluded by name, see Rulings |

## Result

Guard in CMakeLists.txt after `project()`: `if (CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR AND NOT CMAKE_GENERATOR MATCHES "^Ninja")`
-> `message(FATAL_ERROR "Ninja only: configure with -G Ninja (not ${CMAKE_GENERATOR})")`.
Every configure in the listed files passes `-G Ninja` (cmake/ccache_build.sh, docs/dev/testing.md,
docs/src/content/docs/benchmarks.md, harness/{coverage,coverage_mutants,coverage_one,light_share_build,lua_mem,
restore_stall_build,run_all}.sh, tests/{exact_id,name_index,one_id}_mutants.sh, tests/lua_fetch.sh,
tests/test_engine_error_mutant.sh); AGENTS.md carries the rule once; README.md and AGENTS.md build with
`cmake -G Ninja -S . -B build && cmake --build build`. Listed files with no configure or `make` (nothing to change):
harness/light_share_build.sh's build line already used `cmake --build`.

## Rulings I made

- docs/dev/engine-census.md (outside the file set) holds `cmake -S . -B build-census ...` as the rerun of a past
  measurement (gate 0.5 census, commit 8170a3ef6). I treated it as a record and left it unedited; the check excludes
  it by name. The lead may add it to the file set and drop the exclusion instead.
- docs/dev/ccache.md:111 is `cmake -DTEMPLATE=... -P script` (script mode, no configure): the check's pattern
  does not match `-D`-first commands.
- The check's temp tree is under `.scratch/` of the checkout (ignored), removed on exit.
