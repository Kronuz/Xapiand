# strata-pin-catalog: journal

State: waiting on jobs 261121-045943-2b0229 (A2 --fails-without at head 555a48b5f70f; the old job 261120-023044-64c800 was never tagged --for A2, so it did not count).
Quest: .campaign/xapiand/quests/strata-pin-catalog.md (the lead's; never edit it)

## Doing

Committed eef8d64b2: strata pin 397c02d5 -> e3edb426, reproducer from Xapiand:7b89dc9f3. No API
changes affect Xapiand (strata diff touched catalog.hh, index.hh, section_io.hh's unused
chunked_spill, work_pool_executor.hh; no Xapiand use of section_io). Local ASan build of test_pool
under quiet light running; job scripts in ~/.scratch/xapiand/strata-pin-catalog/.

## Next

1. When 261121-045943-2b0229 finishes, confirm A2 met in `campaign status`, then push.
2. CI at 555a48b5f70f: only `Build + unit + smoke` red, and only test_lua.cc:1673 (known flake, lua-method-flake quest owns it); every other job green (ASan, TSan, E2E, arm64, FreeBSD). Not mine.
3. A3: `campaign verify --for A3` reuses build-test 261120-024602-994d61 (passed at f8d8ddcfb43a; no merged file touches CMakeLists.txt pin or tests/test_pool.cc).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-015212-3d9e92 |
| A2 | fails without: 261121-045943-2b0229 at head (old pin 397c02d5; earlier 261120-023044-64c800 exit 134: heap-use-after-free in task::detail::scheduler_vtable); passes at the new pin: `ci:Unit tests under ASan + UBSan` |
| A3 | 261120-024602-994d61 (build-test) + `ci:Functional E2E (docs)` at the head; CI red only from test_lua (known flake) |
