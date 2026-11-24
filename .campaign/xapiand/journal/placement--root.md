# placement--root: journal

State: waiting on jobs 261120-184051-b7da1d 261123-175306-b9f9c4 and 20 mutant jobs
Quest: .campaign/xapiand/quests/placement.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Waiting on jobs at strata ab0e1beb: root tests 261120-184051-b7da1d, 10-minute fuzz
261123-175306-b9f9c4, 20 mutants (below). The d339430c run failed two ways, both fixed:

1. `placement_root_records` "the directory, counter and namespace keys": the test was wrong. It
   still expected a raw `/` (`Ptwitter/user`); ruling 1 stores byte 0, so it now expects
   `Ptwitter\0user`.
2. "an overlong integer" and the fuzz abort (input `BAEDAgEJAQMEAPoAAAMC`, a namespace whose node
   id 0x7a is written `fa 00`): the code was wrong. `varint::leb128::decode` accepts a
   non-shortest form, so the reader took two encodings of one record and the fuzzer's
   round-trip `require` aborted. `codec::reader::number` now refuses a last byte 0 after the
   first. Regression: the crash bytes in `rules()` and in `root_corpus/crash-namespace-overlong-node`
   (replayed every build), a three-byte overlong case, and a new mutant `shortest-form`.

## Next

0. Lead's change (overlong check belongs in varint: varint:333384e; strata's pin bump on
   fix/varint-pin, strata:b4694757). Once the lead says it landed and is merged into
   quest/xapiand/placement: merge that into this branch; remove `codec::reader::number`'s
   shortest-form check; keep the crash bytes in `rules()`, the corpus file and the three-byte
   case (they then prove varint's check); drop the `shortest-form` mutant and cancel
   261120-184104-36c5cc if unrun; rerun root tests and fuzz at the new head. Until then, the
   jobs at ab0e1beb stand.
1. Read 261120-184051-b7da1d, 261123-175306-b9f9c4 and the 20 mutants; fix any failure with a job.
2. Full suite (`build-test`) on the task branch once those pass (CI runs on push).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending at ab0e1beb: 261120-184051-b7da1d (records, placement + model, root group); fuzz 261123-175306-b9f9c4; mutants below |
| A5 | `docs/mechanisms/placement-root.md` (root half; repair's note is task repair's) |

Mutant jobs (`--fails-without`, after 261120-184051-b7da1d, at ab0e1beb): slash-raw
261120-184058-c79fd0, trailing-bytes 261120-184101-2554bf, shortest-form 261120-184104-36c5cc,
count-bound 261120-184107-7f41e3, leader-voter 261120-184109-320ce4, distinct-members
261120-184112-afa37c, distinct-groups 261120-184114-509256, leaders-le-groups
261120-184116-a5361a, erase-counter 261120-184118-da971a, put-unchecked 261120-184120-7d2405,
corrupt-accepted 261120-184122-b8c951, no-expectations 261120-184124-095ee8, remove-busy-node
261120-184126-fc384b, create-existing 261120-184129-3f2ea7, no-pending 261120-184131-30aa04,
any-state 261120-184133-49d82e, delete-keeps-load 261120-184136-567e2b, update-no-version
261120-184139-ba294f, leader-by-groups 261120-184141-bfeec2, no-witness 261120-184144-06d7d2.
Superseded (failed or skipped at d339430c): 261120-181331-7b77ce, 261120-181334-01bf69 and its
19 mutants.

Messages:

| Id | Answer |
|---|---|
| msg-1 (scan -> root, key order) | Done as asked (lead ruling 1, Xapiand:8ff0a0f5f): strata d339430c stores each `/` as byte 0, so keys sort in path order (`twitter` < `twitter/user` < `twitter-a`) and `subtree_begin(p)`/`subtree_end(p)` = [key \0, key \1) follow `p`'s key; `path_of` maps back and refuses a raw `/`. Property test in `root_records_test.cc`; note's "Order" paragraph updated. |

## Result

Not ready. Files: `include/strata/placement/{records,placement,root}.hh`,
`test/placement/root*` (`root.cmake` registers them), `docs/mechanisms/placement-root.md`.

## Rulings I made

- Header-only like `strata::group` (target `strata::placement`); no `src/placement/`.
- `CMakeLists.txt`'s two hunks (target, `test/placement/*.cmake` glob) are now the lead's
  (strata:4a42c30c); this task registers tests only in `test/placement/root.cmake`.
- Everything is a record, the application state is one format byte: counters (`D` next node id,
  `G` next group id) are records the leader reads and rewrites under expected versions; the
  directory version is `D`'s strata version (M5); loads (`L` + id) are records so placement reads
  nodes, not 10^6 namespaces. Commands are conditional record writes prepared at the leader
  (D106's writer); apply decodes, validates every value and writes one transaction.
- Placement: preferred leader = fewest leaderships, other data members = fewest groups; D7's
  witness (rf 2 with a third node) and learner (two nodes). Picking every member by groups first
  breaks "at most one leader above the mean" (model: 2 above at one rf, 100+ mixed), so the
  leader slot deviates from "fewest groups"; groups then drift up to 10 apart under mixed rf.
  Accepted by the lead (Xapiand:8ff0a0f5f).
- The leader is members[0] (a preferred leader, not a hint; D5's "no leader hints" holds).
- Node records keep id out of the value (it is the key); Xapiand's M1 fields go in `attributes`.
- Name uniqueness among unretired nodes (M1) is left to Xapiand's join command.
- Shortest-form integers: checked in `codec::reader` at ab0e1beb as a stopgap; superseded by the
  lead (varint is ours and refuses overlong LEB128 since varint:333384e; strata pinned ac6003e),
  so the reader's check goes once the pin bump lands (Next 0).
- Bench (`bench/placement/**`) not started: the 10^6-record measurements are the quest's
  "When done" jobs.
