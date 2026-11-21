# strata-pin-catalog: journal

State: waiting on jobs 261121-084807-0ed3b0 (A2 head, must pass) and 261121-084810-78fdfc (A2 fails-without at old strata pin 397c02d5, must fail); both at de687295b.
Quest: .campaign/xapiand/quests/strata-pin-catalog.md (the lead's; never edit it)

## Doing

Committed eef8d64b2: strata pin 397c02d5 -> e3edb426, reproducer from Xapiand:7b89dc9f3. No API
changes affect Xapiand (strata diff touched catalog.hh, index.hh, section_io.hh's unused
chunked_spill, work_pool_executor.hh; no Xapiand use of section_io). Local ASan build of test_pool
under quiet light running; job scripts in ~/.scratch/xapiand/strata-pin-catalog/.

## Next

1. When the two A2 jobs finish, confirm A2 met in `campaign status`. The reproducer script is harness/pool_asan_repro.sh (outside the quest's file set: lead widens files). CI note: its ASan job skips test_pool in the parallel ctest but the next step runs `ctest -R '^test_pool$'` alone, so CI does run it under ASan on this branch.
2. CI at 555a48b5f70f: only `Build + unit + smoke` red, and only test_lua.cc:1673 (known flake, lua-method-flake quest owns it); every other job green (ASan, TSan, E2E, arm64, FreeBSD). Not mine.
3. A3: `campaign verify --for A3` reuses build-test 261120-024602-994d61 (passed at f8d8ddcfb43a; no merged file touches CMakeLists.txt pin or tests/test_pool.cc).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-015212-3d9e92 |
| A2 | head (pass): 261121-084807-0ed3b0; fails-without (head's test, strata pinned back to 397c02d5 via STRATA_PIN; the pre-pin commit has no reproducer): 261121-084810-78fdfc; script harness/pool_asan_repro.sh builds at the job's commit with ci.yml's ASan flags and exits 126 on a build break (default CMake generator: the shared .deps sub-builds are Unix Makefiles); the superseded 261121-045943-2b0229 (asan_run.sh) is not evidence |
| A3 | 261120-024602-994d61 (build-test) + `ci:Functional E2E (docs)` at the head; CI red only from test_lua (known flake) |

## Rulings I made

- A2's old-pin run keeps the head's tests/test_pool.cc and edits only the strata GIT_TAG in the job's checkout (STRATA_PIN), since the commit before the pin move lacks the reproducer.
