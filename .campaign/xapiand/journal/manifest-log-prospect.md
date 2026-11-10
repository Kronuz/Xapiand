# manifest-log-prospect: journal

State: waiting on jobs 261109-162807-511d50 261109-162821-d27e0d
Quest: .campaign/xapiand/quests/manifest-log-prospect.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Tests ported and committed in the prospect worktree (head after "Pin strata at the manifest-log
head 9d4630cd"). Locally at strata:4d2a541 the whole suite passed (51 tests); strata moved to
strata:9d4630c (index.hh only), so both jobs run at that pin: build-test 261109-162807-511d50,
A3 identity 261109-162821-d27e0d.

## Next

1. Read both jobs; fix what is red; fill the Evidence rows; write Result.

## Blocked on

Nothing.

## Evidence

One row per criterion, with what its kind needs: a job id (it, or the same job rerun, passed at
the current code), a `path`, a decision id, or for a [manual] one who checked what. Name every job
you examined, failures included, and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|


## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

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
