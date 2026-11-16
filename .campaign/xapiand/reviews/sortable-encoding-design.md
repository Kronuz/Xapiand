# sortable-encoding review: design

Commit: 455d0a447422fadd4d428f2bfdea08e3afa3ff85
Verdict: CHANGES
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

The shape is right: one exact number space across `int64`, `uint64` and `double` is the only design that
serves Xapiand's shared slot, ctype and accuracy prefixes, and the codec's proof is strong. Two things
must change in the design before steps 2 and 3 are built: D8's marker must be durable before the WAL
can hold a record (F1), and T1 and T3 need a CI home that keeps C++ and Python byte-equal after the
switch (F2). The design is on `design/sortable-encoding`; the library is judged at sortable:2e85e15. Clean room kept: I opened neither GPL file, nor the GPL Python module, nor any Xapian
source, nor `design/sortable-codec`'s `legacy_probe.cc`.

## Evidence I relied on

Library jobs, records read with `campaign job show` and the logs where named:

- 261109-145953-7b39eb (sortable:937c7c6, exit 0): `checked 4294967296 floats`, `all floats passed`.
  Every binary32, widened, round trips bit for bit with strictly rising bytes, and every NaN is `ff`.
  Float32's exponents -149..127 reach both escapes and the direct window, with both signs.
- 261109-145956-b3bf94 (937c7c6) and 261109-172754-1f56b7 (sortable:2e85e15, exit 0, log read): 5 of 5
  CTests, including `sortable_property` (exact comparator `test/exact.hh`: frexp for doubles and a shift
  loop for integers, codec-free), `sortable_fixtures` (the 113,491 vectors equal `tools/model.py`'s
  output) and `sortable_cli` (CLI against the model, 213,491 encode and 741,467 decode lines).
- 261109-145954-894c13 (937c7c6, exit 0): one CPU-hour of fuzzing under ASan and UBSan, 363M runs.
  261109-172719-1f0b22 (2e85e15, exit 0): 722 s, 73M runs, with the oracle on both halves (order and
  re-encode).
- The security review `sortable-proof-security.md` (APPROVE at 937c7c6). Its job 261109-163356-a7173d
  killed the 38 `tools/mutants.py` mutants and 4 of its own. The only survivors were the declared
  equivalents.

These settle the codec. Order and round trip are exact, equal numbers give equal bytes across types,
and the decoder accepts only canonical input. I do not rerun them.

## What I checked myself

Probe scripts are in `.scratch/review-sortable-encoding-design/` in the home checkout.

1. **D8 executable model.** Job 261110-034552-09edf8 (`d8_model.py`, pure Python, 7.4 s). It encodes D8 as
   written and enumerates every history of up to 3 sessions. Each session is a G0pre, G0 (step 2) or G1
   (switch) build running up to 2 writes, deletes or commits, then a crash or a clean close: 480,714
   histories. V0 is D8 as written: 13,416 misreads and 13,416 false refusals outside Limit 1 (a
   history that later downgrades to G0pre counts under Limit 1, so these are lower bounds).
   Shortest traces: `G1:create w crash G0:accept` and `G1:create w crash G1:refuse`. V1 commits the
   marker at creation and adoption: 0 and 0 outside Limit 1. F1 and F8.
2. **Double rounding of string-parsed floats.** Job 261110-033430-7f2f96 (`run_double_round.sh`, x86_64). It compares
   `std::strtod` with `(double)std::strtold` on fixed and random decimal strings. F3.
3. **sortable as a subproject.** Job 261110-033431-f36daa (`run_subproject.sh`, sortable:2e85e15 via `--ref`). It
   configures a parent project that adds sortable the way FetchContent does, lists which targets exist,
   and greps Xapiand's workflows for Python. F2.
4. One local dry run of `d8_model.py` (seconds, before the job). The job is the record.

Answers to the named questions:

1. **D1's case against the 8-byte map: sound.**
   - The pigeonhole is right: `int64` with `uint64` hold 2^63 + 2^64 integers, more than 2^64 codes.
   - The four meeting points hold at 432299ed0:
     - one `DB_SLOT_NUMERIC` for floating, integer and positive (`database/schema.cc:2292-2302`);
     - ctype `N` for all three (`database/schema.h:421-424`), with the slot `xxh32(prefix, ctype)`
       (`database/utils.cc:102-104`);
     - an integer given to a floating field goes through `floating()` (`serialise.cc:417-418`);
     - floats' accuracy terms are `static_cast<int64_t>` (`database/schema.cc:5359-5368`).
   - The f64 map merges integers above 2^53, so `N` id terms would collide. That is a wrong document,
     not a wrong order. Per-type maps break meeting points 1 to 3. Q1 (a) is the only exact choice; the
     owner took it (D44).
2. **D3, D5 and the order proof.**
   - I checked the 16 example encodings by hand against D3's rules: 1 `be`, 5 `c040`, 0.01
     `b747ae147ae147b0`, DBL_MIN `810034`, DBL_MAX `fe03bffffffffffffff0`, -5 `3ec0`, -2^63 `02`,
     -5e-324 `7effff`, the infinities and the rest.
   - The proof sketch holds. 2^(8n) - F keeps F's lowest set bit, so a negative's tail keeps its length
     and its nonzero last byte. -(2^U) has the empty tail and sorts first in tag U. The escapes bound
     the direct tags on both sides.
   - D5's nine rules are `sortable.hh:288-339`. D3 and D5 with the mechanism note are a complete spec
     for a clean-room port (D7).
3. **Order across types (question 1): proven at the library.**
   - Values above 2^53: boundaries 2^k + d for k 0..63 and d in [-1024, 1024], both signs.
   - Subnormals: every 2^e for e -1074..1023 with `nextafter` neighbours, plus random bit patterns.
   - -0.0 is `80`, and NaN is `ff` above +inf, with NaN refused at index time (D4, D44).
   - Cross-type equality is asserted per item against the exact key.
   - The gap is in Xapiand's own paths (F3, F4), not in the codec.
4. **Prefix-freeness (question 2): not needed where it is not provided.**
   - Single-value `memcmp` is correct with prefixes: a prefix sorts first, which is the numeric order.
   - The key maker frames multi-key sorts (`multivalue/keymaker.cc:761-784`):
     - forward: `00` becomes `00 ff`, ended by `00 00`;
     - reverse: each byte is complemented, `00` becomes `ff 00`, ended by `ff ff`;
     - the last forward key goes in raw.
   - Ranges compare whole slot values. Accuracy terms are exact-equality lookups. A StringList's `00`
     never starts an encoding.
   - Size against today: F7.
5. **Migration (question 3): the per-shard marker reaches every store of codec bytes.**
   - It covers values, terms, geo accuracy terms (`multivalue/generate_terms.cc:339, 493`), dates, ids,
     versions (`engine/strata/shard.cc:632`) and the WAL, whose records are serialised documents
     (`database/wal.cc:276`).
   - It also covers the registry and foreign schemas. They are documents in `.xapiand/indexes`, a shard
     like any other (`database/schemas_lru.cc:309-313`).
   - There is one open path (`database/shard.cc:494`). Dumps carry documents only (`database/handler.cc:87`).
   - The metadata API's only write verb is PUT: update and delete answer 501 (`server/search_views.cc:1198-1213`).
     So no restore or API call can plant or remove `_format`.
   - The holes are F1 and F8; F10 is the registry's message. Limit 1 (builds older than step 2) is
     accepted.
6. **The Python port (question 4).** T3's content is strong:
   - every vector;
   - 10^6 seeded inputs;
   - decode accept and reject agreement over every 0-2 byte string and every escape-tagged 3-byte
     string, the reject witnesses and 10^6 mutations.

   It keeps nothing equal "forever" while it has no CI home (F2).
7. **What must be true before the owner freezes it (PLAN open question 8, D30).**
   1. F1 and F2 fixed. T1 to T10 green at the frozen commit, with T3 in CI on every push, so an edit to
      the client or a pin bump that changes bytes fails a check.
   2. The bytes named by a digest: the sha256 of the pinned library's `test/vectors.txt` and
      `test/reject.txt`, recorded in `docs/mechanisms/value-encoding.md` and asserted by Xapiand's C++
      and Python tests. The pin points at a tagged sortable commit whose `tools/model.py` reproduces
      them.
   3. A checked-in fixture per generation, as the PLAN's gate rule asks ("readers reject unknown format
      versions with a fixture per format change", `.campaign/xapiand/PLAN.md:82-84`): a small
      generation-1 shard with documents and a WAL that every later build must open, and a generation-0
      one it must refuse.
   4. D8's rule after the release decided. "Accept only my own generation" holds while formats are
      open. Once real data lives in generation 1, later builds read it or upgrade it in place; refusing
      it would strand that data.
   5. T7's on-disk and cost numbers (F7), and T4's fuzz hour at the pinned library commit (done at
      937c7c6 and 2e85e15; rerun if `decode` changes).
   6. The semantics that change answers but not bytes decided: ±inf and open bounds, NaN bounds (F4).
      Phase 7b's replicas and remote shards carry the marker.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | D8 makes the marker durable only at the shard's first commit. It "writes it through the engine" (`engine/shard.h:118`, a pending write), and T8 says "a new shard has `_format` after its first commit". Writes reach the WAL before that commit, and WAL records carry codec bytes (`database/wal.cc:276`). So a generation-1 shard that crashes before its first commit is, on disk, an unmarked shard with a WAL. The switch build then refuses its own WAL-acknowledged writes; deleting the shard is the only way out. Worse, a step-2 build checks, reads the absent marker as 0, accepts the shard and replays generation-1 bytes. That is the misread D8 exists to stop, in a build D8 means to protect ("every build from then on checks"). An adopted shard has the same window. | Job 261110-034552-09edf8, V0: 13,416 misreads and 13,416 false refusals outside Limit 1, shortest `G1:create w crash G0:accept` and `G1:create w crash G1:refuse`. V1: 0 and 0. The check runs before the replay, `database/shard.cc:494, 518-529`. | Make the marker durable before any WAL record can exist. On creation and on adoption, set `_format` and commit before `open_index_blocking` returns (the WAL's revision bookkeeping, `database/wal.cc:161-249`, must accept that first revision). Or carry the generation in the WAL header (`database/wal.cc:81`) and check it when the marker is absent. Build it in step 2. T8 adds create, write, kill -9, then a reopen by the same build (accepted, replayed) and by the other generation's build (refused), and the same after an adoption. Confirm by rerunning the model with the chosen rule as V0. |
| F2 | yes | gap | T1 and T3 have no rerunnable home in Xapiand's checks, so nothing keeps C++ and Python byte-equal after step 3. sortable builds `sortable_cli`, `sortable_property` and its tests only as the top-level project (sortable `CMakeLists.txt:10`), so Xapiand's build-test cannot run A2's `sortable_property` or T3's harness. Xapiand's CI runs no Python. The design names no source for the client test's `vectors.txt` and `reject.txt`. A later edit to the client's module or a pin bump could change one side's bytes with no check failing. A2 and A9 cannot be met as designed. | Job 261110-033431-f36daa: added as a subproject at sortable:2e85e15, only the interface target `sortable` exists; `sortable_test`, `sortable_property`, `sortable_size` and `sortable_cli` are absent. No workflow in `.github/workflows/` at 455d0a447 names python or pytest. | Name T3's home in the design. Recommended: Xapiand's CI checks one frozen fixture in both languages. A C++ test runs the pinned library's `test/vectors.txt` and `test/reject.txt` (read from the fetched source, never a copy) through `Serialise::`/`Unserialise::`. A Python step runs the client's test over the same files. Both assert the files' sha256. The CLI differential becomes a CTest that builds `sortable_cli` from `${sortable_SOURCE_DIR}/tools/cli.cc`. Alternative: the port lives in Kronuz/sortable beside `model.py`, is tested in that repository's CI, and the client vendors it with a digest check. Point A2 at the chosen home. |
| F3 | no | defect | D6 narrows `long double` to `double` after the parse of a string. `Serialise::floating(std::string_view)` calls strict_stold, which is `std::strtold` (`serialise.cc:592`, strict-stox `strict_stox.hh:1466`). That rounds twice where `long double` is wider (x86-64's 80 bits, arm64 Linux's binary128) and once on arm64 macOS. So one decimal string can index to different bytes per platform. It can also differ from the same number sent as JSON, and from a string range bound, which `Cast::floating` parses with strict_stod (`cast.cc:219`). | Job 261110-033430-7f2f96: on x86_64 (`long double` 16 bytes, 64-bit mantissa), `9007199254740993.0000001` gives 2^53 + 2 through strtod and 2^53 through strtold. Random decimals of 15, 16, 17 and 19 significant digits differ in 206, 243, 224 and 230 per 10^6, about 1 in 4,300. | Parse straight to `double` (strict_stod or `std::from_chars`) and drop the `long double` overloads. T6 adds a near-midpoint decimal string such as `9007199254740993.0000001`, as a value and as a bound. |
| F4 | no | gap | T6 misses cases the exactness claim rests on. (a) Negative fractions: pre-1970 datetimes with sub-second parts, -(2^a) with fraction bits. (b) Meeting point 2, D1's own case: one field typed integer in one index and floating in another under one wide search. (c) ±inf with open bounds: `getNumericQuery` fills an absent bound with the query type's limits (`multivalue/range.cc:92-93, 106-107, 120-121`), so `x >= 5` on a floating field ends at DBL_MAX and leaves +inf out. A NaN bound passes `val_s > val_e` and encodes `ff` (`range.cc:95-100`). | file:line | Add (a) to (c) to T6. D6 says whether an absent bound is unbounded. D4's 400 covers NaN bounds. |
| F5 | no | gap | Geo accuracy terms change bytes (`Serialise::positive`, `multivalue/generate_terms.cc:339, 493`). T5's real-corpus identity covers only numeric and datetime sorts and ranges. | file:line | Add a seeded geo query set over cities to `harness/sort_range_identity.py`. |
| F6 | no | gap | T5's 200,000-file corpus is a live walk of a home directory (`sizes.py:88-101`), so a rerun at the same commit sees other files. | file:line | Freeze it as an ndjson with a digest, and have T5, T7 and F7 read that. |
| F7 | no | gap | Question 2, size against today. The real-data table compares fixed 8 and 10 B, not today's codec. The library's `test/size.cc` bounds synthetic means against the superseded design's old-codec numbers (`size.cc:24, 34-51`). Index size waits for T7 on main after landing, and no disk-cost number is planned. | design D2, `sizes.txt` | On-disk size is a byte count, so it can be a branch claim before landing: `du` after FLUSH of the restored accounts, cities and the frozen file corpus, old build against new. T7 keeps the timing and states the disk delta in dollars or watts for the freeze. |
| F8 | no | gap | D8's "an empty shard is adopted" is unsafe if "empty" means no live documents. A shard whose documents were all deleted is then adopted with the other generation's deleted documents still in its segments. | Job 261110-034552-09edf8: 7,280 such adoptions over 480,714 histories (informational) | Define empty as nothing on disk that carries codec bytes: no segment and no WAL. |
| F9 | no | nit | The metadata listing unserialises every value as MsgPack (`server/search_views.cc:1108-1113`). A raw `"1"` lists as 49 (0x31 is a MsgPack fixint), and a later `"10"` may break the listing. | file:line | Store `_format` as MsgPack like other metadata, or leave it out of the listing. |
| F10 | no | nit | The registry `.xapiand/indexes` holds every index's settings and foreign schema, with versions in `DB_SLOT_VERSION` (`database/schemas_lru.cc:309-313`, `index_resolver_lru.cc:313-340`). D8 refuses it like any shard, which refuses every index. Its message ("restore it with this one") does not fit, since settings and schemas are set again on a new node (`upgrade.md:19-24`). | file:line | Check the registry's marker at startup and exit like the Xapian check: status 65, name the index, "start on a new data directory and restore each index". Add a T8 case. |
| F11 | no | nit | T1 says 10^7 random values per type, but the CTest runs 3x10^6 with a fixed seed (sortable `test/property.cc:312`), so CI repeats one sample. | file:line | State the CI count, or vary the seed per run and print it. |
| F12 | no | nit | T3's random half draws integers uniformly in [-2^63, 2^64), so nearly all of them are 8- and 9-byte encodings; short tails rest on `vectors.txt` alone. | design T3 | Draw integer magnitudes log-uniformly, as `tools/model.py:197` does. |

Notes for other designs, outside this quest:

- Phase 7b's replicas and remote shards must carry and check `_format`. D8's "single open path" holds
  only until then.
- "Changes to other designs" item 1 leaves two number encodings: prospect's typed columns with fixed
  per-type maps, and Xapiand's byte-slot codec. phase-4 D3a should say which Xapiand fields, if any,
  use typed columns, so no later quest adds a third.

## Not run

- No Xapiand build. T3, T5, T6, T7 and T8 are step 3's (and the lead's) work.
- F1 is shown by a model of D8's rules plus the cited open and WAL code, not by crashing a built server.
- F3 was measured on x86-64 only. The arm64 Linux (binary128) and arm64 macOS behaviour comes from their
  `long double` widths, not from a run.
- I did not reread the sortable CI runs (42717337992, 42737478382), the superseded sortable-codec design
  or its review, or `legacy_probe.cc`. D1's case stands without them.
- I did not open any GPL file, the GPL Python module, or any Xapian source.
