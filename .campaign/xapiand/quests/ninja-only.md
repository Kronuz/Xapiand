# ninja-only: Xapiand builds with CMake and Ninja only

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status ninja-only`.

## Outcome

Xapiand configures only with a Ninja generator (owner, 2026-11-21: "The rule for these particular
projects is using CMake and Ninja for builds. Any other way is plain wrong"; decision D168). CI,
release, FreeBSD and the Homebrew formula already pass `-GNinja`. But the instructions and scripts
agents copy did not: Xapiand's AGENTS.md and README.md, and the harness, bench and test scripts this
quest lists. Every configure there omitted `-G`, so CMake chose Unix Makefiles. In a job slot, a
Makefiles tree and a Ninja tree sharing `.deps` (FETCHCONTENT_BASE_DIR) then break each other at
configure ("generator : Ninja does not match the generator used previously").

1. **Guard.** The top-level `CMakeLists.txt`, right after `project()`, stops at configure with a one-line
   message naming `-G Ninja` when `CMAKE_GENERATOR` does not start with `Ninja`. It does this only
   when Xapiand is the top-level project, so a consumer's FetchContent build is untouched.
2. **Instructions and scripts.** Every listed file that configures CMake passes `-G Ninja` and builds
   with `cmake --build` (or `ninja -C`), never `make`. Records of past runs (docs/gates, docs/perf,
   docs/reviews, docs/status, results) keep the commands that were run and are not edited.
3. **The rule is written once,** in Xapiand's AGENTS.md build section: "Build with CMake and Ninja
   only: every configure passes `-G Ninja`; the top-level CMakeLists refuses any other generator."

## Acceptance criteria

- A1 `harness/ninja_only_check.sh` configures a fresh tree with `-G "Unix Makefiles"` and its own
  FETCHCONTENT_BASE_DIR, so it shares no `.deps`, and exits 0 only when configure fails with the
  guard's message. It runs as a job, passing at the fix and failing without the guard
  (`--fails-without` at the quest's base)
- A2 The suite passes at the landed code, configured with `-G Ninja`: a job
- A3 [artifact] No file in Xapiand outside the records above configures CMake without Ninja:
  `harness/ninja_only_check.sh` also greps for that and lists any it finds, so A1's passing job covers it

## Contracts

Building Xapiand with Make, Xcode or any non-Ninja generator now fails at configure. The owner decided
this (D168).

## Decisions

- D168: Ninja only, enforced at configure; the campaign's jobs default to it through
  `campaign config env=CMAKE_GENERATOR=Ninja`.

## May land early

None.
