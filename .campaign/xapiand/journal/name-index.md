# name-index: journal

State: fixing F1, F2 (correctness review, round 1 of 2)
Quest: .campaign/xapiand/quests/name-index.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Correctness review (.campaign/xapiand/reviews/name-index-correctness.md, CHANGES at 291ec89), round 1:
- F1 fixed: a lookup counts one visit per name read, a seek none; a dead name waits on a stack (`closed`) until its
  subtree, which is skipped unread; a dead prefix that is no name is found by its first name (that visit is its
  entry). Each name read maps to a distinct entry the walk reads, so names <= walk. A name steps only the segments
  it does not share with the last name stepped (`levels`, `ends`), so steps <= walk too. Test:
  test_the_names_never_refuse_what_the_walk_answers (random trees at caps 1-40, nested 50+50 at cap 50); mutant
  no-closed.
- F2 fixed: `index_pattern::Unreadable::fail` (the build's walk) throws on an opendir error other than ENOENT and
  ENOTDIR (ruling D110); `build()` returns false, keeps `failure()`, and the pool logs it; the walk keeps answering
  and a later pattern retries. Test: test_an_unreadable_directory_fails_the_build (EMFILE by taking every
  descriptor, then a retry equals the walk); mutant skip-unreadable.
- F3-F6: not fixed (non-blocking), for a follow-up:
  - F3: delete needs `erased()` with a tombstone the build applies at its swap; erasing `twitter` keeps
    `twitter/user` (D9).
  - F4: the delta is unbounded until the first pattern; say so in D4 and the Limits, or merge before completion.
  - F5: on a case-insensitive file system `opened()` adds the request's spelling and the walk lists the disk's.
  - F6: a build that keeps failing is retried, and logged, by every later pattern; it needs a back-off.
  - Also seen: a readdir error in the middle of a directory still reads as the end of it (D110 covers opendir).

## Next

1. Submit at the fix: targeted tests, mutants no-closed and skip-unreadable (--fails-without), the reviewer's
   oracle.sh and emfile.sh, and build-test and e2e via `campaign verify`. Then ready for review.

## Blocked on

Nothing. Lead rulings received (docs to "index names", lazy build stays, one parser, release gating is the
owner's): 1 and 3 done in xapiand:6dcae4a, 2 kept, 4 noted.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/name-index.md` |
| A2 | 261117-110258-76c693; fails without: 261117-110228-88596e |
| A3 | 261117-110158-de9685 (build-test, async_check included), 261117-110158-9cdd8c (e2e), 261117-110204-e7835e; fails without: 261117-110214-031049, 261117-110217-85a114, 261117-110221-d6fa49, 261117-110224-f3f320 |
| A4 | 261117-110204-e7835e; fails without: 261117-110232-ddea0a, 261117-110235-4fbeb9, 261117-110238-651fe2 |
| A5 | 261117-110204-e7835e; fails without: 261117-110242-e44601 |

## Result

Index patterns resolve from `NameIndex`, an in-memory sorted set of the walk's names, built by one walk in a merge
chore after the first pattern and kept by the catalog's resolve; the walk answers until the build ends. A prefix
pattern takes 2.1 µs at 10^4 to 10^6 indexes against the walk's 8.2 to 875.6 ms (422,592x faster at 10^6); the build
takes 167.5 s at 10^6; 16 B per name (261117-110258-76c693). Docs: docs/design/name-index.md,
docs/mechanisms/wide-search.md. Open: the release gating (owner).

## Rulings I made

- R1 In memory, rebuilt by one walk at every start; no persisted table (design D1). The directory stays the one
  truth, so A5 has nothing to reconcile.
- R2 The build starts at the node's first pattern (lazy), not at boot (lead ruling: a walk at start costs every
  node with millions of indexes). Until it ends, patterns walk.
- R3 The lookup keeps `--max-search-indexes` as its bound, counting names read and seeks as visits (D7); `_all`
  over the cap stays 400. The walk gate does not apply to lookups.
- R4 Resolve adds on open as well as create, so an index copied in while running is listed once opened (D3).
- R5 The build is a detached task of the control loop awaiting one `merge` chore (lead finding 1, phase 7c D13). No
  request awaits it; patterns walk until it ends. With no control loop nothing is posted. Clearing the control stops it.
- R6 The names' 6 lock sites stay, classified `allow`: taken only on pool workers (resolve's open chore, the build
  chore, a lookup's query chore), no I/O or await under them; loops touch only atomics.
- R7 F1's "same 400 text when both refuse" cannot hold as asked: the names read less than the walk, so they can
  get to the indexes cap (too many names, "indexes" text) where the walk refuses its work first ("directory
  entries"). Probe: 37 of 2,869 both-refuse cases at caps 1-40, all names=indexes cap, walk=work. The test asserts
  what holds: a work refusal by the names implies one by the walk, and the work texts differ only by the noun
  (the lead's earlier ruling). For the lead to confirm.
