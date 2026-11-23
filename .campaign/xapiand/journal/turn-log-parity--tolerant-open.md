# turn-log-parity--tolerant-open: journal

State: waiting on jobs 261121-145607-0e1ac9 261121-104707-bd7691 261121-104711-fedf27 261121-115239-ee5107 and CI at strata:a16d117c
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

strata:a16d117c pushed (quest/xapiand/turn-log-parity--tolerant-open); jobs queued, built against
journal-lost-bound (journal:1e3505a, `--ref journal=...`, `-DFETCHCONTENT_SOURCE_DIR_JOURNAL`).

## Next

1. Read the jobs; paste `campaign brief`'s table under Evidence.
2. Landing needs journal-lost-bound landed first, then strata's journal pin bumped to it (the
   strata CI builds today's pin, which lacks `GroupStatus::bounded`, until then).

## Blocked on

Nothing in this task. It depends on quest journal-lost-bound (the option 1 chosen from the
earlier Blocked on), which carries its own file-set request and reviews.

## Evidence

| Criterion | Evidence |
|---|---|
| A2 | 261121-145607-0e1ac9: `test_tolerant_open` at strata:a16d117c, journal:1e3505a. `--fails-without`: 261121-104707-bd7691 at the strata mutant strata:62f31fc7 (refs/scratch/tolerant-open/opens-every: attach opens every bounded index; fails U and W); 261121-104711-fedf27 at journal:1d773a5 (every named group lost: today's rule; fails C and D). 261121-115239-ee5107: standalone_turn_log_test against the journal branch |

Local (RelWithDebInfo, quiet light): tolerant open 0 failures.

## Result

strata:a16d117c.

- `attach_log` (include/strata/core/index.hh): a `GroupStatus::bounded` group opens when the head's
  `cur_.applied.index` reaches `Host::bound`, replaying the log's records after the head as for a
  stored group; otherwise `Host::lose` and `log_lost`. `turn_sealer` forwards `bound`/`lose`
  (include/strata/core/turn_seal.hh).
- `test/test_tolerant_open.cc`: one damaged 4 KiB block holds C's, U's and W's 600-byte batches
  (sequence 4 each), the start of U's 8 KiB filler (5) and D's commit checkpoint (3). Bounds
  asserted exactly: C 4, U 5, W 4, D 3. C (committed through c2) opens with k0, c1, c2 and writes
  c3; D opens with k0, d1; U and W fail `log_lost` and report lost, also after a host reopen,
  where C's later checkpoint has cleared its bound. Damaging the next block's header too (the
  manifest) leaves no bound: C, U and W all fail `log_lost`.
- Mechanism note: journal `docs/mechanisms/lost-bound.md` (who may replay past a bound: strata's
  index, whose head records its last applied sequence).

## Rulings I made

- Coverage test is bound <= head's applied index: a commit installs the head only after the head's
  records are sealed under the lease, so every record at or below the head's index is in the head.
- W (bound, no later write) and U (later write) both lose: neither head covers its bound.
- Damage to the manifest too loses every index (no bound source); a known limit until a copy
  flag or per-segment roster gives a bound, as the journal note's Limits say.
- Did not reproduce today's failure with a job earlier: it is a direct code path; the
  `--fails-without` at journal:1d773a5 is that reproduction now.
