# The name index: index patterns from an ordered set of names

> Status: DRAFT (2026-11-17) for quest name-index (carried "Index patterns at scale", wide-search SR1, 2026-11-03).
> Not reviewed. Builds on X-POOL's catalog (`docs/design/x-pool.md`): its resolve is the one place an index's
> directory is opened or created.

## Summary

A search's index patterns (`_all`, `logs/2026/*`, `tenant/**`) resolve today by walking the data directory
(`index_pattern::expand`, `docs/mechanisms/wide-search.md`) under a budget of `--max-search-indexes` entries. Every
pattern reads the entries of every directory it can match below, so `w*` on a node with a million top-level indexes
reads a million entries, and a node with more indexes than the cap cannot take a wildcard at all.

The name index is the walk's answer kept in memory as a sorted set of names. A pattern becomes a few range scans
of it: the bytes before a pattern's first wildcard are a range, and a subtree that no pattern can match below is
skipped with one seek. A prefix pattern costs O(log N + names read) instead of O(entries on disk). The data
directory stays the one truth: the set is built by one walk of it, kept current by the catalog's resolve, and rebuilt
at every start, so nothing new is persisted and a crash leaves nothing to reconcile.

## Decisions

**D1. In memory, owned by the pool, never persisted.** `NameIndex` (`src/database/name_index.{h,cc}`) is a member
of `DatabasePool`, beside the catalog whose resolve keeps it. A persisted names table was the alternative; it is
rejected:
1. It is a second truth beside the directories, so every create needs an intent record synced before the
   directory is made, and every start reconciles the records against the disk; the in-memory set needs neither.
2. It still misses what the node did not create (an index directory copied in by an operator) until something
   rescans, and the rescan is the walk this design already runs once at start.
3. strata keeps no partition-wide list of indexes to borrow: a cold index is its directory alone (no slot, at most
   16 B in memory, `strata/docs/design/cold-tier.md` T1), and the catalog holds only the open ones.

The cost of that choice is the start: the set is complete only after one walk of the whole data directory (D5).

**D2. The set is exactly what the walk lists.** The names are those `expand(root, {"**"})` returns: a directory
path whose segments are valid names (`index_path::valid`) not starting with `.`, with no segment but the first
equal to `strata`, that holds a `strata` directory or a shard `.__N` (not a regular file). Nested indexes are the
rule, not a case (D22, owner 2026-11-03): `twitter` and `twitter/user` are two names, and a name's descendants
follow it in the order.

**D3. The catalog's resolve keeps it.** Every resolve of a key on disk (an open or a create, `make_host`'s
`h.resolve`, after `create_directories`) calls `NameIndex::opened(key)`, which adds what that directory makes the
walk list: for each proper prefix P of `key + "/strata"`, P is an index when the segment after it is `strata` or
starts with `.__`, and P is added when D2 admits it. So `twitter/.__2` adds `twitter`; `a/strata` adds `a` alone
(the walk takes `a/strata` for `a`'s own data and never lists it), and `a/.__1/b` adds `a`. A name already present costs a
lookup, O(log N) under a shared lock; only a new one takes the exclusive lock. An open adds too, so an index
copied in while the node runs is listed once anything opens it. The hostless path (`engine::open_local`, engine
tests and `engine::check`) never serves a request (x-pool CR6) and keeps no names.

**D4. A packed sorted base and a small delta.** The base is one byte array of the names in order and their end
offsets (`std::uint64_t`), so a name costs its length plus 8 bytes. New names go to a delta, an ordered
`std::set`, about 64 bytes each. When the delta passes max(1,024, base / 8) names it is merged into a new base, an
O(N) copy under the exclusive lock (about 25 MB, a few milliseconds, at 10^6 names), so a create pays O(8 names'
bytes) amortized. A lookup reads both in one merged order. Front coding (names share long prefixes under nesting)
would cut the base further; it waits for the A2 numbers.

**D5. Complete after one walk; the walk is the fallback until then.** The first pattern a node serves starts the
build on a thread of its own: one unbounded walk (`expand(root, {"**"})`, every directory a pattern could reach),
whose names become the base, merged with what resolve added meanwhile. A resolve during the build is never lost:
an index on disk before the build started is found by it, and one made after is added by its resolve. Until the
build ends, and after it fails (logged; the next pattern starts it again), patterns walk as before, under the same
bounds. `DatabasePool::finish()` stops a running build at its next directory entry.

**D6. A lookup is ranges, a merged scan and seeks past dead subtrees.** For each wanted pattern its literal
prefix is the bytes before its first `*`, `?` or class `[...]`, less a trailing `/` (`a/**` matches `a`). The
prefixes are sorted and one that extends another is dropped, so the ranges are disjoint. In each range, every
name is stepped through each pattern's `index_pattern::Glob` one segment at a time; a name is listed when a
wanted pattern accepts it and no exclusion does, which is the walk's rule (D2's set, filtered as
`Walk::matched` filters). When no wanted pattern is live after a segment of a name, its prefix up to that segment
is dead, and the scan seeks to the first name past that subtree (`prefix + '0'`, since `0` follows `/`). Plain
names, exclusions and the checks on the items go through `index_pattern::parse`, the one parser the walk uses too.
The cost is O(R log N + names read), R the ranges, at most 32; the names read are the matches, the names in a
range that a pattern rejects, and one per dead subtree.

**D7. The same bounds, counted in names.** A lookup counts each name it reads and each seek as a visit, each
pattern step as `expand` counts it, and the names it lists; past `--max-search-indexes` visits or names, or the
steps, it raises the same `errc::invalid` (400) as the walk. So `_all` on a node over the cap is still refused,
while `w00*` on a node of 10^6 indexes reads only the `w00` range. A lookup holds the shared lock for its scan and
runs in a query chore, never on a reactor; it skips the walk gate (one walk at a time, `--search-walk-queue`),
which bounds the walks only.

**D8. Crash behavior: nothing to recover.** Nothing of the index is written. After a kill at any point of a create
(before `create_directories`, between it and the index's first files, or after them), the next start's build
reads what is on disk and lists exactly what the walk lists: a directory left with an empty `strata` is listed by
both, and a search that names it meets a missing index as it does today. The test kills a child process that
creates indexes through the pool, then builds the index over what is left and compares it with the walk (A5).

**D9. What changes around the node is seen late.** The data directory belongs to the node while it runs. A
directory added behind its back is listed when it is opened or at the next start; one removed behind its back
stays listed until the next start, and a search over it skips it as it skips a missing plain name. Xapiand has no
index delete today (`DELETE /index` answers 501); when it lands it calls `NameIndex::erased(name)` after removing
the directory.

## Costs

| What | Cost | Mechanism |
|---|---|---|
| Memory per index | name length + 8 B in the base; about 64 B more while in the delta | D4 |
| A prefix pattern | O(log N + names read) | D6 |
| `_all` | O(N) names read, bounded by the cap | D6, D7 |
| A resolve | O(log N) shared; O(log N) exclusive for a new name; O(N / 8) bytes amortized at a merge | D3, D4 |
| The build | one walk of every reachable directory entry, once per process | D5 |

The build's time is the walk's: 100,000 entries took 1.4 s warm and 5 s cold (SR8), and an index directory holds a
few entries (`strata`, the WAL, its children), so 10^6 indexes are about a minute warm. The quest's A2 jobs measure
it at 10^4, 10^5 and 10^6 indexes beside a prefix lookup in the index and the same pattern walked.

## Limits

| Limit | Value | Mechanism | Reason |
|---|---|---|---|
| Names read by one lookup | `--max-search-indexes` | D7: each name read and each seek is a visit | the work of one lookup has a number, as a walk's does |
| Names listed | `--max-search-indexes` | D7 | the runaway cap, unchanged |
| Delta before a merge | max(1,024, base / 8) names | D4 | a create's amortized cost stays a few names' bytes; the delta stays small beside the base |
| Builds at once | 1 per node | D5: one thread, started once, restarted only after a failure | the build is a whole walk |

## Steps

1. This design, the index (D2 to D7) with resolve's hook, the search's use of it, and its tests: a property test
   that every lookup equals the walk over random nested trees (A3), a node over the cap answering a wildcard
   (A4), and the kill during create (A5).
2. The measurement at 10^4, 10^5 and 10^6 indexes (A2), and front coding of the base if the bytes per index say so.
