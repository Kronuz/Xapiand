# xapiand-strata-bump: journal

State: waiting on jobs 261112-101724-de5872 261112-101724-33f78b 261112-101724-6a3348 261112-101725-a119a1; CI Xapiand 43278295136, prospect 43278199249
Quest: .campaign/xapiand/quests/xapiand-strata-bump.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead's msg (strata main 86fe7c47, unpin-after-kick): prospect pin moved on its branch
`quest/xapiand/xapiand-strata-bump` (prospect:61e09a6, pushed, ff of prospect main a938de6; prospect CI run
43278199249). Merged master (Xapiand:a56be7bcc), pinned strata 86fe7c47 and prospect 61e09a6 (Xapiand:c3fec9b40), pushed.
Rerun at c3fec9b40: A2 261112-101724-de5872, A1 261112-101724-33f78b, A3 261112-101724-6a3348,
mutant 261112-101725-a119a1. Xapiand CI (A4) run 43278295136 (ASan+UBSan, TSan).

## Next

1. Read the four jobs; paste `campaign brief`'s table under Evidence; check CI on both branches (A4).
2. Prospect branch must be green and landed by the lead (ff of prospect main) before this lands.
3. The lead rules on A3's wording (Blocked on); then "ready for review".

## Blocked on

Resolved by strata:86fe7c47 (was: catalog race, TSan run 43217395714). Still open for the lead: A3's wording (the node opens
every index under `defer_io`, which forces eager segments, so the damage surfaces at open and the test proves report plus
heal in that form; the lead restated A3 on 11-12). The prospect pin is a branch commit; landing it is the lead's.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261112-042225-aad4d1 (pins_check: ok) |
| A2 | job 261112-042224-6d8e89 (running) |
| A3 | job 261112-043246-c284b3 (queued), mutant 261112-042232-481834 (killed: exit 0) |
| A4 | ci:Unit tests under TSan failed on 05cba6694 (run 43217395714): strata catalog race, Blocked on; cite ci:<job name> rows once green |

## Result

- CMakeLists.txt: strata ab0e26a6 (main: lazy default, open-one-flush), prospect fcd9f3e (main: completion's
  read-only opens; pins strata d8ae10b8). The "but strata" exception in the pins comment is gone. No consensus in the
  tree, so no consensus pin.
- tests/test_engine_error.cc `test_a_damaged_segment_answers_corrupt_until_healed`: pool path, footer byte at size-30
  of the one seg-*.sx flipped; two requests answer 500 with errc::corrupt; after the byte is restored a request past
  the backoff reads "a". Mutant (tests/test_engine_error_mutant.sh): corrupt_segment as errc::database.
- Open: the node's per-open cost still maps every segment (defer_io) and runs check_fields over each dictionary.

## Rulings I made

- Hunt row "Xapiand IO subclasses implement completion's four read-only opens": Xapiand has no `kronuz::journal::IO`
  subclass (src and tests use `kronuz::journal::PosixIO` and `strata::faultfs::io`, which implement them at these
  pins), so nothing to forward; the build at the new pins is the proof (A2).
- A1 "agree": Xapiand pins the head strata (ab0e26a6), of which prospect's d8ae10b8 is an ancestor; pins_check's A3
  passes. The lead named strata main; pinning d8ae10b8 instead would drop open-one-flush.
- The mutant script restores its source on exit, so it runs in the quest's build slot without --fresh.
