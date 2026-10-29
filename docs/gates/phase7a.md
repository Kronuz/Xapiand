# Gate 7a (S5): Xapiand on strata

Run on Xapiand master `9fa890ecc` (SF merged), 2026-10-28, Apple Silicon macOS, one binary for both engines
(`-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -DLTO=OFF -DBUILD_TESTS=OFF`), the
engine chosen per run by `XAPIAND_ENGINE` (D8's override). newman 6.2.3. Rows are from `xapiand-on-strata.md` section 5.
Functional runs under `quiet light`, the T11 timing under `quiet exclusive`. Nothing was fixed; every failure is recorded
for the lead.

## Results

| Row | Target | Result | Status |
|---|---|---|---|
| T6 E2E on strata | Zero assertion regressions against the Xapian baseline | Xapian 576/576, strata 507/576 (339 requests, 0 uncovered; the same 507 on three strata runs). 69 failing assertions: 53 on 27 `INFO` requests (R-P2), 2 on the `_terms` aggregation over a text field (R-P2 reading disputed, see D2), 14 on four searches that use PHRASE, NEAR or ELITE_SET (no ruling covers them). One more regression the assertions miss: R1 below | FAIL (16 assertions and R1 without a ruling) |
| T6 `verify_fields` open cost (CR3) | Measured | 4 to 7 merged segments per shard, 50 fields: 0.10 to 0.15 ms per shard open (100k documents) and 0.07 to 0.14 ms (500k documents), 0.6 to 1.3 ms on the first cold open after a restart; the first search after a restart (five shards open) takes 70 ms (100k) and 108 to 124 ms (500k). At most 3.6% of that search, and it does not grow with the document count | PASS (recorded) |
| T7 bodies | Every difference listed and justified | 41 of 339 bodies differ; all listed below. 37 are covered by R-P2 or are volatile or are wording; 3 are database-info length bounds with no ruling (D3); 1 is a regression (R1). Hit order, `total` and `count` identical on all 89 responses with hits | FAIL (R1) |
| T8 crash recovery | Same document set as Xapian after `kill -9`; strata digest check passes | A: 50 PUTs without a commit, `kill -9`: both engines recover the same 50 documents with every `n` right, and the restart logs the WAL replay. B: a writer PUTs sequentially, `kill -9` after 4 s: Xapian recovered 5366 of 5366 acknowledged writes, strata 7399 of 7399, none lost, none duplicated, all intact. The strata check (`index::check`, every file's digest, plus the field table) passes on all 5 shards right after each kill and after each restart and second kill (4 of 4 check runs, five shards each). A flipped byte in a segment fails the check (`section checksum`) | PASS |
| T9 migration | `_dump` on one engine, `_restore` on the other, both ways, equal dump | bank, 1000 documents. Xapian to strata: the two dumps are identical. Strata to Xapian: identical except `_version` of 5 documents (2 instead of 1). The same 5 differ when the Xapian dump is restored on Xapian (control), so the difference is the Xapian restore, not strata | PASS with a note (D4) |
| T11 replacement wins | Engine share per document at most half of Xapian's, CPU and critical path; parity fails | Strata's engine share is 0.80x Xapian's in CPU (0.322 against 0.404 ms per document, 20% less) and 0.53x on the critical path (0.042 against 0.079 ms per document, 47% less). Neither is at or under 0.5 | FAIL |

## T6 failures, classified (strata report against Xapian's 576/576)

`harness/engine_e2e.sh`; failures grouped by the response each returned (all `400`):

| Count | Requests | Response | Class |
|---|---|---|---|
| 53 assertions | 27 `INFO` requests (`INFO /test/namespace/doc`, `/twitter/tweet/1`, `/assets/Lenna`, `…/doc.terms.name`, and the like) | "The strata engine keeps no term lists; a document's terms are not offered" | By design: R-P2 ("`DocView::terms`, `term_details` and the `:info` term view are not offered on the strata engine (refused with a clear error)") |
| 2 assertions | `SEARCH /bank/` with `_aggregations: {most_used_terms: {_terms: {_field: "personality"}}}` | the same term-list message | R-P2 says `_terms` is served from stored values (P1); `personality` is a text field, whose terms are tokens, not stored values. Needs a ruling (D2) |
| 4 + 4 | `SEARCH` with `personality: {_phrase: ...}` and `{_near: ...}` | "does not search the PHRASE (NEAR) operator yet" | Not covered by a ruling: S2 delivers PHRASE and NEAR over terms; the text parser hands them over as an append chain (`validate` in `src/engine/strata/search.cc:198-206` refuses a non-joining operator in an append chain). A regression against Xapian and unfinished S2/S4 scope |
| 2 + 4 | `SEARCH` with `personality: {_elite_set: ...}` and an `_elite_set` list of field terms | "does not search the ELITE_SET operator yet" / "ELITE_SET of anything but terms" | Not covered by a ruling: P3/S3 list ELITE_SET (`xapiand-on-strata.md:88, 281`), and the engine searches it over plain terms (`search.cc:182-189`) |

### R1 (regression the assertions do not catch)

`RESTORE /twitter/tweets/` (two documents, after `PUT /twitter/tweets/` with the Twitter `_schema`, whose `_id` is
`integer`): Xapian indexes both; strata indexes the first and answers the second with
`{"status":500,"message":"Cannot update schema while preparing: Too many retries"}` (`prepared:1, indexed:1`). It
happens in all 3 strata suite runs and not once on Xapian. Minimal repro (`.scratch/t6r/repro2.sh`, copied to the main
checkout): `PUT /twitter/tweets/` with that schema, then `RESTORE` the two tweets as JSON; 2 of 2 tries on strata fail,
0 of 2 on Xapian. Restoring into a fresh index without the schema works (0 of 30 failures on either engine). The suite's
assertion is "Response is success" (HTTP 200 of the batch), which passes.

## T7: all 41 body differences (`harness/bodydiff.py xapian.json strata.json`)

1. 27 `INFO` requests, status 200 to 400: R-P2.
2. 5 `SEARCH /bank/`, 200 to 400: the 4 phrase, near and elite-set searches and the `_terms` aggregation above.
3. `INFO /bank/`, `GET /test_replication/:info`, `INFO /twitter/tweet/`: `doc_len_lower` and `doc_len_upper` are 0 and 0
   on strata and 4294967114 / 244, 4294967288 / 8, 4294967284 / 15 on Xapian (the lower bound is a wrapped unsigned
   value, 2^32 minus a small number). Every other key (`doc_count`, `doc_del`, `av_length`, `last_id`, `has_positions`,
   shards) is equal. No ruling covers it (D3).
4. `INFO /some/resource/name`, `POST /some/resource/name`, `GET /some/resource/name:info`: both 404; the message is
   "No glass database found at path 'some/resource/.__1'" on Xapian and "No such database: some/resource/.__1" on strata
   (D9, errors are engine kinds). Wording only.
5. `POST /twitter/user/`: generated id. Volatile.
6. `GET /:metrics`: counters. Volatile.
7. `RESTORE /twitter/tweets/`: R1.

Scores, ties and estimates: `harness/search_order_diff.py` compares what the bag comparison hides. All 89 responses
that carry `hits` have the same hit order, `total` and `count` on both engines (no `_score` is returned by the suite, so
ranking is compared through order; the five refused searches carry no hits).

## T8, T9, T11 detail

T8 (`harness/engine_crash_check.sh`, `tests/strata_check_tool.cc`). The server's `:check` endpoint cannot run the strata
check: `DatabaseHandler::check` calls `engine::xapian::check` whatever the index's engine (`handler.cc:264-272`), so on a
strata index it answers "Directory does not contain a Xapian database" for every shard (D1). The digest check therefore
runs through a 40-line tool linked against the server's objects, calling `engine::strata::check(path)` with no server
up. Build recipe: compile `tests/strata_check_tool.cc` and `tests/test_stubs.cc` with the flags of
`CMakeFiles/xapiand.dir/flags.make`, and link with `CMakeFiles/xapiand.dir/link.txt` with `main.cc.o` replaced by the two
objects. Replay is lazy on both engines: the script writes to every shard, commits, then lists the set.

T9 (`harness/engine_migration_check.sh`). A `DUMP` with any explicit `Accept` header is a 406 on both engines; with none it
is a JSON array. Records are compared sorted by `_id`, in canonical JSON. Two nodes run at once, one per engine; the
engine of each index is checked on disk (five `strata` directories, five glass databases).

T11 (`harness/profile_bulk.sh`, now with a strata layer and an engine-share line). 20000 accounts documents by one
RESTORE, `?commit`, one fresh node per run, `sample(1)`; four interleaved pairs (xapian, strata, xapian, ...). The rows
counted are those the harness marks "replaced S1": Xapian's `Xapian::Document` build and engine; strata's
`engine::strata::` and `strata::`/`prospect::` frames (the strata run also has 0.1 to 2.4% in Xapian frames, the
system indexes, which are added to its share). The A/A noise is the spread of each engine's own four runs.

| Metric | Xapian, 4 runs | Strata, 4 runs | Strata / Xapian |
|---|---|---|---|
| Engine CPU per document, smoke CPU x share (ms) | 0.418 0.418 0.395 0.384, mean 0.404, spread 9% | 0.332 0.332 0.311 0.314, mean 0.322, spread 6% | 0.80 (pairs 0.79 0.79 0.79 0.82) |
| Engine CPU per document, from samples (ms) | mean 0.427, spread 11% | mean 0.345, spread 10% | 0.81 |
| Engine on the critical thread per document (ms) | 0.082 0.082 0.081 0.073, mean 0.079, spread 12% | 0.042 0.043 0.043 0.040, mean 0.042, spread 6% | 0.53 (pairs 0.52 0.52 0.53 0.55) |
| Engine share of CPU / of critical thread | 35.2% / 34.6% | 38.4% / 27.6% | 1.09 / 0.80 |
| Whole process CPU per document (ms) | 1.147, spread 2% | 0.838, spread 1% | 0.73 |
| Bulk docs/s | 4465 | 5200 | 1.17 |

Direction: strata costs less than Xapian in every row, and the engine rows alone are 20% less CPU and 47% less on the
critical path; the target (at most half) is met in neither. The whole process is 27% cheaper per document because the
rest of the pipeline (schema, term generation, the WAL) shrinks too, but that is not the row. Strata's share of its own
CPU is higher (38% against 35%): its biggest frames are `StrataDocument::term_of`, `basic_index::commit_files` and
`StrataDocument::~StrataDocument`.

## Fixes the rows found

None; the lead asked for no engine fixes. Harness changes on this branch: `bodydiff.py` printed only the first 40
divergences and now prints all; `profile_bulk.sh` gained the strata layer and the engine-share lines; new
`engine_e2e.sh`, `engine_crash_check.sh`, `engine_migration_check.sh`, `search_order_diff.py` and
`tests/strata_check_tool.cc`.

## Deviations and the ruling that covers each

| ID | Deviation | Ruling |
|---|---|---|
| D1 | `:check` is not engine-aware: on strata it reports every shard as not a Xapian database | None. A seam gap (X4 "get/find_key/stats/metadata/check"); the lead decides |
| D2 | `_terms` aggregation over a text field refused | R-P2 covers the `INFO` views; for `_terms` it says "served from stored values", which a text field has no equivalent of. Needs a ruling: refuse for text fields on strata, or serve from the dictionary |
| D3 | `doc_len_lower` / `doc_len_upper` of database info are 0 on strata | None. Xapian's lower bound is a wrapped value, so a plain match is not wanted; decide the field |
| D4 | `RESTORE` of a dump into Xapian bumps `_version` of 5 of 1000 documents to 2 (also Xapian to Xapian) | Pre-existing Xapian behaviour, not strata's; T9 compares strata's dumps strictly and the round trip ignoring `_version`, with the control |
| D5 | PHRASE, NEAR and ELITE_SET over text refused (14 assertions, 4 searches) | None found. Design S2/S3/S4 deliver them; `strata-engine-text.md` decision 1 and `search.cc` validate show the refusal |
| D6 | 404 wording for a missing database | D9 (errors are engine kinds) |

## Residuals

1. T6, T7 and T11 fail; the lead decides each: D2, D5 (finish the operators over text, or rule them out), R1 (a bug in the
   strata RESTORE schema path), and the T11 target against the measured 0.80x and 0.53x.
2. R1's cause is not investigated; the repro is two requests.
3. The strata engine share carries `StrataDocument::term_of` (the host's term conversion) as engine work, as the Xapian
   side carries `Xapian::Document` building; both are the rows "replaced S1".
4. T8 B measures acknowledged writes against the async WAL window (2 s grace, 0 older writes missing, 0 newer missing in
   this run); the grace is not a durability claim.
5. CR3 was measured with a local timing patch (not committed) on 100k and 500k documents; larger dictionaries were not tried.
6. Each T11 number is one hold of four pairs on one machine (Apple Silicon); no other platform.

## Commands

```sh
export PATH=$HOME/.scratch/newman6/node_modules/.bin:$PATH      # newman 6.2.3
cmake -S . -B .scratch/build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS=-fno-omit-frame-pointer -DLTO=OFF -DHAVE_CLOCK_GETTIME=1
cmake --build .scratch/build -j4 --target xapiand
harness/engine_e2e.sh .scratch/build/bin/xapiand OUT                 # T6: OUT/{xapian,strata}.json, assert and diff text
python3 harness/bodydiff.py OUT/xapian.json OUT/strata.json          # T7
python3 harness/search_order_diff.py OUT/xapian.json OUT/strata.json
BIN=.scratch/build/bin/xapiand harness/engine_crash_check.sh          # T8 (CHECK_TOOL=.../strata_check)
BIN=.scratch/build/bin/xapiand harness/engine_migration_check.sh      # T9
XAPIAND_ENGINE=strata harness/profile_bulk.sh BIN OUT --label strata_1 --port 18990   # T11, under quiet exclusive
```
