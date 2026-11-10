# manifest-log-prospect: journal

State: ready for review
Quest: .campaign/xapiand/quests/manifest-log-prospect.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Done. Jobs at prospect ff99494 (strata pinned at strata:9d4630c).

## Next

1. Lead: review; repin strata to manifest-log's landed main commit at landing.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261109-162807-511d50 (51 of 51) at ff99494: reclaim, media (every byte of the log), directory, fixtures, format13 and index/lifecycle run on the log; the dropped crafted-file cases and the strata test covering each decoder half are under "Dropped cases" below |
| A2 | build-test 261109-162807-511d50 at ff99494: CMakeLists.txt pins strata 9d4630cd7eed22974676f67d2cff08a8e8014eef (manifest-log head; repinned by the lead at landing) |
| A3 | build-test 261109-162807-511d50 (51 of 51) and identity job 261109-162821-d27e0d at ff99494 (ctest 51/51; churn and dump identical to strata main 3df7af4 at flush threads 1 and 2) |

Dropped cases (R1), each with the strata test at strata:9d4630c covering its decoder half
(`test/manifest_log_test.cc` stage names; chosen by reading the stages, not by a mutant run):

- bitset size wraps near 2^64: stage "decoder", the blocks "One encoding per deleted set"
  (`short_bitset` is corrupt: 64 documents take 8 bytes) and "A count no input can hold" (`lie`:
  a 2-byte bitset for 2^62 documents is corrupt); stage "bounds on read".
- delta wraps back onto an ordinal: stage "replay checks", `rejects("a new ordinal below the
  document count")` and `rejects("a new ordinal not yet deleted")`; stage "decoder", the "lie" case
  (deltas claiming 2^62 deletes in 2 bytes); stage "bit flips" (every flip refused or equal).
  A wrapped delta is not constructible through `log_deleted::of`, so prospect cannot re-craft it.
- live docs / metadata of another generation, another index, another segment (resealed): no file
  to swap. The log's binding is the blocks naming their log's generation: kept in prospect as
  index_test "a log under a newer name" ("no whole snapshot") and the install damage "a snapshot
  under another generation's name"; strata: stages "crash images" and "bit flips".
- the two install damages that wrapped a delta or bitset size: same as the first two rows.

## Result

prospect's tests read and damage strata's manifest log; behavior unchanged: 51 of 51 tests pass
and churn and dump output equal prospect on strata main at flush threads 1 and 2 (jobs above).
Changed: test/*_test.cc (reclaim, media, directory, fixtures, format13, index, lifecycle, reorder,
background_merge, churn), fixtures (manifest-log.bin replaces manifest/live/meta .bin;
pre_freeze.txt files row 0x8000'0008), CMakeLists.txt pin. Open: the pin moves at landing.

## Rulings I made

- R1 (retired-file cases): the live-docs, metadata and `fields-*` files no longer exist, so the
  index_test cases that crafted them have no file left to craft: "live docs of another
  generation / another index's / another segment, resealed", "bitset size wraps near 2^64",
  "delta wraps back onto an ordinal", "metadata of another generation (resealed)", and the two
  install damages that wrapped a delta or a bitset size. Their decoder halves are strata's
  manifest_log_test ("bounds on read", "bit flips") and fuzz targets. Kept as log cases: deleted
  ordinals past the document count and a live count unlike the deleted set (install), the
  resealed document and live counts, the snapshot under another generation's name, a flipped
  snapshot byte, and the segment file cases. "A resealed live length" is now out of step with the
  index total (the segment's alone), since the log holds the length once.
- R2 (format13 F20): `rewrite` reads the generation's state, applies the edit, `recount`s and
  writes `frame_snapshot` as the generation's own log; the `fields = {1, 10, 0}` file reference
  becomes a later field declaration under an unknown mask bit; "a cache attribute leaves at the
  next commit" checks the replayed head state (the log keeps the old snapshot's bytes, so the
  raw `find("tier")` has no meaning).
- R3 (fixtures): the files family's `.bin` goldens (manifest, live, meta) leave with their codecs;
  `manifest-log.bin`, a keyed index's log with `identity_seed = 1` (bytes fixed), replaces them in
  C17's files row; `pre_freeze.txt` re-pinned at `files::pre_freeze` 0x8000'0008.
- R4 (D6.6): the open runs no retention, so reorder_test, background_merge_test and index_test
  call `reclaim()` where they expected the open's orphan removal, as strata's tests do; the
  open's first-read arm of the pinned-log EIO case is gone with it.
- R5 (applied): `ix.manifest().applied` is the head's; the live position the tests compared is
  `ix.applied()`.
