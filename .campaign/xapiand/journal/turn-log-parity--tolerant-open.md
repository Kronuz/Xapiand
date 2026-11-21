# turn-log-parity--tolerant-open: journal

State: blocked on lead: A2 needs a journal change (Detent's lead reviews it)
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

## Doing

Nothing: stopped at the journal step, as the assignment says.

## Next

1. Lead rules on the options under "Blocked on" and routes the journal change (a journal task,
   or this task drafting it on a journal quest branch for Detent's lead).
2. Once journal reports bounds: strata `attach_log` (index.hh, `GroupStatus` switch near :3365)
   opens a bounded group when bound <= `cur_.applied.index`, replaying the records after the head,
   and otherwise marks it lost and throws `log_lost`.
3. Test `test/test_tolerant_open.cc` (rocksdb-parity A2): one log, groups C (head past the damaged
   block), U (bound above its head), W (no bound source); damage one block with faultfs. C opens with
   every record, U and W fail `log_lost`. `--fails-without` at a mutant that opens every bounded
   group (fails U) and at the base (today's rule fails C).

## Blocked on

A2 cannot be done strata-side. At journal 178030b (strata's pin) the loss is decided and made
durable before any index opens:

1. `TurnLog::recover` (turn_log.h:555-745) puts every group the damaged block's manifest names
   (`gap_damage`, :605-611; the manifest holds each group's highest sequence in the block,
   framing.h:89) into `TurnRecovery::lost` and drops its records. Its only spare is
   `superseded(group, seq)`, seq <= `TurnLogGroups::base`, which `Host::known` fills for closed
   groups only (host.h:554).
2. `Host::open` then `mark_lost`s those ids durably and removes their artifacts (host.h:607-629),
   so the index's own files go before strata can compare its head. strata's attach only sees
   `GroupStatus::lost` (index.hh:3365).

So C (head covers every damaged record) fails `log_lost` today, and no strata change can spare it.
journal main (335b1b5) is the same: `TurnRecovery` has no bound.

Options (who decides: lead, with Detent's lead reviewing the journal diff):

1. **Recommended: recovery reports a bound (rocksdb-parity A2 items 1-2).** Journal: a group the
   damaged block's manifest names gets `TurnRecovery::bounded[group] = highest sequence the damage
   could hold` instead of `lost`; its records after the bound are kept (contiguous from bound + 1
   or from a later checkpoint, no need to start at 1), and its next sequence is at least the bound.
   Host: `GroupEntry` stores the bound durably at open (beside `mark_lost`, without removing
   artifacts), so it survives reclamation of the damaged segment and re-partition until the index
   attaches; `status()` reports it (a new `GroupStatus::bounded`, or `stored` plus `bound(group)`);
   `clear_bound(group)` after the index proves coverage, `mark_lost(group)` when it does not.
   Exempt bounded groups from the "damaged, open, nothing kept" rule (turn_log.h:741). Unknown
   damage (no manifest) keeps losing every group until task 3's copy flag or task 14's roster give
   a bound. Journal size: about 60 lines plus its tests; strata: about 30 lines plus the test.
2. **A per-open-group base in the table.** strata reports each installed head's covered sequence
   (`Host::advance_base`, a durable table write per checkpoint); `known()` passes it for open
   groups and the existing `superseded` check spares C. Costs a second durable write per
   checkpoint per index (millions of indexes per host), changes `base`'s meaning for open groups,
   and has no path to the copy-flag bound. Not recommended.
3. Keep `log_lost` for all (D103 option 1 as it stands). Contradicts the quest's outcome.

## Evidence

| Criterion | Evidence |
|---|---|
| A2 | <none: blocked on the journal change above> |

No jobs run: the cause is in code (journal 178030b turn_log.h:605-611, :741; host.h:554,
:607-629; strata index.hh:3365), not a hypothesis to measure.

## Result

Nothing changed yet in strata. Diagnosis above.

## Rulings I made

- Did not reproduce with a job: the failing rule is a direct code path (journal marks the group
  lost and removes its artifacts at host open), and a reproduction test needs the journal API
  that option 1 adds; it becomes A2's test once that lands.
