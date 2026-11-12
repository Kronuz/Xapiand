# xapiand-strata-bump: journal

State: waiting on jobs 261112-042224-6d8e89 261112-042225-aad4d1 261112-042228-ee73ed 261112-042232-481834
Quest: .campaign/xapiand/quests/xapiand-strata-bump.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs queued at quest/xapiand/xapiand-strata-bump 3cdf295b2 (branch pushed for CI, A4):
- 261112-042224-6d8e89 build-test check (A2)
- 261112-042225-aad4d1 configure + harness/pins_check.py (A1); local run: `pins_check: ok`, strata head ab0e26a6,
  prospect's own pin d8ae10b8 listed as a stale pin inside prospect (ancestor; 24 stale library pins, none Xapiand's)
- 261112-042228-ee73ed tests/test_engine_error.cc alone (A3); local run: OK 144 checks
- 261112-042232-481834 mutant tests/test_engine_error_mutant.sh, --fails-without (A3)

## Next

1. Read the four jobs; paste `campaign brief`'s table under Evidence; check CI on the branch (A4).
2. The lead rules on A3's wording (Blocked on); then "ready for review".

## Blocked on

Lead: A3 as written ("opened under the lazy default ... corrupt at first read") does not hold anywhere in Xapiand:
- The node opens every index under `defer_io` (src/engine/strata/shard.cc:171), and strata forces eager segments under
  it (strata:ab0e26a6 include/strata/core/index.hh:603, "a first-read map would be I/O inside apply()").
- The hostless path (open_local, check) and `borrow` both call `Core::verify_fields()` at open, which maps every segment
  and scans its dictionary directory and pool (shard.cc:291, 474, 1614). An experiment dropping it from `borrow` changed
  nothing: the pin still failed with "corrupt segment: footer checksum" (defer_io).
So a damaged segment fails the pin's open: the client gets 500 corrupt from the request that opens the index, the
catalog backs off (10 ms doubling) and keeps no damaged index, and after the repair a request past the backoff reads it.
The test proves that form. Options:
1. Accept A3 in that form (recommended: it is the node's real behaviour and covers report + heal).
2. Make the node lazy: strata maps lazily under defer_io outside apply() (a strata quest), and the field check moves
   from Xapiand's open to prospect's first map. Worth measuring for the many-indexes per-open cost first.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261112-042225-aad4d1 (pending) |
| A2 | job 261112-042224-6d8e89 (pending) |
| A3 | jobs 261112-042228-ee73ed, mutant 261112-042232-481834 (pending) |
| A4 | CI on quest/xapiand/xapiand-strata-bump 3cdf295b2 (pending) |

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
