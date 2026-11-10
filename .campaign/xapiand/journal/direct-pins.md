# direct-pins: journal

State: waiting on jobs 261109-180009-09ece8 261109-180009-9ee666 261109-180022-38e01d
Quest: .campaign/xapiand/quests/direct-pins.md (the lead's; never edit it)

## Doing

Commit Xapiand:c09b43121 on the branch: eight Xapiand pins moved to the newest commit any resolved
library pins, override comments reworded, harness/pins_check.py added. Jobs run build-test, e2e and the
pins check at that commit.

## Next

1. Read the three jobs; if build-test or e2e fail, bisect the eight bumps (a bump that breaks is a stale
   consumer: record it under Blocked on, keep the old pin only with the lead's ruling).
2. Fill the Evidence table, set State to ready for review.

## Blocked on

No Xapiand-side blocker. A3 cannot hold in every library until each stale pin below is bumped in its
own repository (the lead files them; `harness/pins_check.py --strict` lists them and is the proof). Each
line is library: stale pin -> head it needs.

- cuuid: char-classify 57613ed349 -> 25decb57ef; endian a24942eaba -> 6b223e2ed8
- url_parser: char-classify 57613ed349 -> 25decb57ef
- md5: endian a24942eaba -> 6b223e2ed8
- storage: compressors 5c5412871a -> 5566260a30
- strata: compressors 5c5412871a -> 5566260a30; hashes e3bf5b6167 -> 03ab2d69f7; completion e2c64747df -> 9acfba342b;
  threadpool eb4751a33e -> 63e03a5dec
- msgpack: constexpr-phf 1d82461008 -> 4b669b6c06; enum-reflection 653fb34844 -> a7202ada8d;
  hashes e3bf5b6167 -> 03ab2d69f7; repr 87450b43c6 -> d2eb8955d9
- fs: split 40af1a2089 -> d48a28e50a; strings 1be38483a3 -> 6799a6e8a1
- soundex, string-similarity, system, traceback: strings 1be38483a3 -> 6799a6e8a1
- traceback: term-color 94df6e5e1c -> 76a400be21
- prospect: strata 9d52824eb2 -> 5d742b2407
- datetime: strict-stox b947d77073 -> 1f9190ca19
- residency: task 6558c2e9c5 -> bdafa5f1ca

Heads are the newest commit any resolved library pins (ancestry checked in the resolved trees); a bump
in one library changes its tree, so the script is rerun after each (heads can move).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261109-180022-38e01d (pins_check.py A1 table: 57 declarations, each with an include site) |
| A2 | job 261109-180022-38e01d (pins_check.py A2: no comment justifying a pin by winning) |
| A3 | job 261109-180022-38e01d (Xapiand's pin is the head for all 57 declarations; library-side stale pins listed above, to be bumped in their repositories) |
| A4 | jobs 261109-180009-09ece8 (build-test) and 261109-180009-9ee666 (e2e) |

## Result

Xapiand:c09b43121 changes CMakeLists.txt only (plus harness/pins_check.py). Every Kronuz declaration
already named a library a Xapiand source includes (A1 table), so none was deleted: the "declared first"
statements were comments over legitimate declarations, now reworded. Eight pins were older than one a
library pins and are bumped to it: char_classify, compressors, constexpr_phf, endian, random, split,
static_string, threadpool. 23 stale pins remain inside libraries (Blocked on).

## Rulings I made

- A2 read literally: a declaration of a directly included library is not an override; what is removed
  is the claim that it exists to win, and any pin older than a library's. The declaration stays because
  FetchContent needs a declaration to fetch a directly included library at the head.
- "Head" = the pin every other pin is an ancestor of, checked with git in the resolved trees.
- A3 is proved by a separate job (the quest's checks are build-test and e2e, which do not run the script).
