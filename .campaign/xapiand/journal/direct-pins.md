# direct-pins: journal

State: waiting on jobs 261110-032054-0d5177 (build-test) 261110-032054-4e0406 (e2e) 261110-032056-a735b0 (pins check) at xapiand:adfe69916 (rebased onto master after query-dsl-fixes; jobs at 08e2c5e82 cancelled)
Quest: .campaign/xapiand/quests/direct-pins.md (the lead's; never edit it)

## Doing

e2e 261109-180009-9ee666 FAILED at Xapiand:c09b43121 (42 assertions, from "Load sample data" RESTORE /bank/ with no
HTTP response; INFO then shows doc_count 895 of 1000). build-test 261109-180009-09ece8 and pins check
261109-180022-38e01d passed. Finding: all eight bumps are doc/CI-only in their libraries (`git diff --stat old new` per
library: compressors LICENSE+ci.yml; threadpool docs/status+ci.yml; char-classify, endian, constexpr-phf, random, split,
static-string: ci.yml only), and `git diff 625963a89 HEAD` outside .campaign is CMakeLists.txt + harness/pins_check.py. So
no bump changes compiled code. The passing master e2e (261109-152541-04dbf7) ran at 4f289be1c, which lacks three src/harness
commits that this branch's base 625963a89 has (ad641a000, 72203782d, 711a9b6c6). The harness server log was lost: the tls
layer overwrote /tmp/xe2e_node.log. Two jobs decide it: e2e at the base 625963a89 (4ceb58) and a rerun at the head (118818).

## Next

None: ready for review.

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
| A1 | job 261110-032056-a735b0 at Xapiand:adfe69916 (pins_check.py A1 table: 57 declarations, each with an include site) |
| A2 | job 261110-032056-a735b0 (pins_check.py A2: no comment justifying a pin by winning) |
| A3 | job 261110-032056-a735b0 (Xapiand's pin is the head for all 57 declarations; 23 library-side stale pins listed in Blocked on, to be bumped in their repositories) |
| A4 | build-test 261110-032054-0d5177 and e2e 261110-032054-4e0406 passed at Xapiand:adfe69916 (ed84c3759 differs by the journal only). The earlier red e2e runs were the fixed-port collision (see Rulings), shown by the concurrency check in Rulings. |

## Result

Xapiand:c09b43121 changes CMakeLists.txt only (plus harness/pins_check.py). Every Kronuz declaration
already named a library a Xapiand source includes (A1 table), so none was deleted: the "declared first"
statements were comments over legitimate declarations, now reworded. Eight pins were older than one a
library pins and are bumped to it: char_classify, compressors, constexpr_phf, endian, random, split,
static_string, threadpool. 23 stale pins remain inside libraries (Blocked on).

## Rulings I made

- e2e collision (lead's finding; 261109-205508-a1b8ee and 261109-192429-4ceb58 were SIGKILLed by a concurrent
  e2e, and 9ee666 likely the same): xapiand:8677ec94f gives each e2e_capture.sh run an OS-assigned port (PORT still
  pins one) and a private mktemp work directory (collection, node log, newman log were fixed /tmp paths); a squatter
  on an explicit PORT is killed only if it LISTENs on it (lsof -sTCP:LISTEN, else ss); run_all.sh (smoke, load,
  stress, bench, tls), tls_check.sh and wal_recovery_check.sh take free ports too. docs_to_postman.py takes the
  base URL from XAPIAND_URL, which the capture already sets from PORT, so no collection edit was needed.
  Test: harness/e2e_concurrent_check.sh (two captures at once, both GREEN); run against the old capture script it
  should fail, which shows the test sees the bug (jobs below).

- A2 read literally: a declaration of a directly included library is not an override; what is removed
  is the claim that it exists to win, and any pin older than a library's. The declaration stays because
  FetchContent needs a declaration to fetch a directly included library at the head.
- "Head" = the pin every other pin is an ancestor of, checked with git in the resolved trees.
- A3 is proved by a separate job (the quest's checks are build-test and e2e, which do not run the script).
