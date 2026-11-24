# prospect-pool-reader-transient: prospect: the field check reads the pool through a reader it drops, so an opened index keeps no pool reader

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status prospect-pool-reader-transient`.

## Outcome

`fielddict::reader::check_fields` scans the field pool through a reader it builds and drops, read
through open_at's checksum verifier, instead of `pool()`, which caches a pool reader for the life of
the open dictionary. An index that is opened and checked but never queried keeps no pool reader:
about 400 B per segment less (field-check-at-map's contributor measured 1.6 KB per index at 1,000
segments with 4 fields and 4.0 KB with 10, about 7% of the heap per open index, on main too). A
query that needs the pool still caches it through `pool()` as before. Refusals and results are
unchanged. The many-indexes workload (millions of open indexes per host) is why it matters.

## Acceptance criteria

- A1 A test shows by value that `check_fields` leaves the pool reader uncached (heap or a cache
  probe, before and after the check), that a later query caches it as before, and that a corrupt
  pooled field id is still refused with the same error. A mutant that scans through `pool()` again
  is killed by the named test (`--fails-without`).
- A2 The suite passes: the `build-test` check, and CI on every platform. [ci] for the platforms.
- A3 Heap per open index before and after at named commits on prospect's own open-only bench (or a
  harness the quest adds), with an A/A, `strict`: the saving per segment and per index at 4 and 10
  fields, the honest magnitude, in `docs/perf/fielddict-pool-reader.md`; open time stays within the
  A/A band.
- A4 [artifact] `CHANGELOG.md` names the change.

## Contracts

None changed: `fielddict::reader`'s public API and the bytes on disk are unchanged.

## Decisions

- Lead, 2026-11-24: from field-check-at-map's fix round (hunt list); its contributor's patch is at
  ~/.scratch/xapiand/field-check-at-map/src/prospect-fix (include/prospect/fielddict.hh, a
  `transient` reader factored out of `dictionary`). Start from it; it was measured through strata's
  eager_open counting build only.
- Lead: risk normal (no format, no concurrency, no decoder change: the verifier path is the same);
  one `correctness` review.

## May land early

Nothing: the change is one function and its test.

## Review

One `correctness` review: does every path that reads the pool after the check still get a verified
reader; can the transient reader outlive `ctx` or the blocks it views; is the refusal unchanged.
