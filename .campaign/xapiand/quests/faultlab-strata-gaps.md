# faultlab-strata-gaps: faultlab's faultfs covers every call strata makes (before migration step 3)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status faultlab-strata-gaps`.

## Outcome

strata's copy (`include/strata/core/faultfs.hh`, 804 lines) gained three members on 11-16 and
11-17 that faultlab never took: `power_on()` (a process crash at `power_off_after`'s cut, after
which operations succeed again; strata:6e455aca), `operations(op kind)` (counts per kind, failed
ones included; strata:18ccec2b) and `sync_latency(d)` (each file sync takes at least `d`;
strata:94d73236). `test/kv_strata_batch_test.cc` and `test/standalone_turn_log_test.cc` call them.
faultlab covers each: an existing operation where one has the same meaning (`kill()`,
`mutations()`, ...), else a new member with its test. Two behaviors found in strata's copy are
settled in faultlab too (Xapiand hunt rows at :100-101). After this lands, strata can move onto
faultlab with include and namespace changes only (faultlab-adopt-strata).

## Acceptance criteria

- A1 Every faultfs member strata calls has a faultlab counterpart, listed in the journal as a
  table (strata member, faultlab member, same meaning or the difference), and each added member
  has a test with values: the faultlab suite job
- A2 Every case in strata's `test/faultfs_test.cc` has a counterpart in faultlab's tests (the
  journal maps them), so deleting strata's copy loses no case: the faultlab suite job
- A3 The two behaviors from strata's copy, each a faultlab test: a sync that fails after its effect
  on a staged file written after the mark leaves the flushed bytes, not the staged snapshot; a
  reclaim-candidate open of a missing name returns null as io.h says (read-only and read-write).
  Where faultlab needed a fix, a `--fails-without` job at the unfixed code
- A4 Adding members moves no seed: faultlab's recorded seed table (F3) is unchanged: the suite job
- A5 [artifact] `docs/api.md` names each added member and what it models
- A6 [ci] CI is green on every platform

## Contracts

Provides faultlab:faultfs (additions only; journal and consensus pin it and must not change).

## Decisions

- Lead, 2026-11-20: a strata member with a faultlab equivalent maps onto it; no alias or second
  name for one operation (one way to do it).

## May land early

Nothing.
