# name-index: journal

State: waiting on jobs 261117-124521-4bdb34 261117-124522-f50a08 261117-124524-bb4872 261117-124540-0da619 261117-124543-44a244 and 11 mutants
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

1. Jobs at 74faa0d: build-test 261117-124521-4bdb34; e2e 261117-124522-f50a08; targeted 261117-124524-bb4872;
   review probes oracle 261117-124540-0da619, emfile 261117-124543-44a244. Mutants: no-closed 261117-124528-4ae707
   (F1), skip-unreadable 261117-124532-538de8 (F2), no-seek 261117-124536-b9339b, no-parents 261117-124554-af9429,
   keep-slash 261117-124558-1a5727, drop-delta 261117-124602-aed1e3, lose-merge 261117-124605-ff8815, no-range
   261117-124609-74470b, no-index 261117-124613-d1d955, no-hook 261117-124617-c3436e, top-build
   261117-124620-59c725. Local probes before submitting: oracle has 0 differences and 0 refusals; emfile is 5 of 5
   incomplete with no silent miss. When green: Evidence from `campaign brief`; ready for review.

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
| A2 | 261117-182056-98ff05, 261117-211000-af5385; fails without: 261117-082444-aa8b4a, 261117-110228-88596e, 261117-124536-b9339b |
| A3 | 261117-182100-25609d (the build-test check), 261117-182100-25609d, 261117-182100-a7b005, 261117-210955-f5e031; fails without: 261117-082425-28cde9, 261117-082430-ea3fab, 261117-082434-4426d7, 261117-082439-942a8a, 261117-110214-031049, 261117-110217-85a114, 261117-110221-d6fa49, 261117-110224-f3f320, 261117-124528-4ae707, 261117-124532-538de8, 261117-124554-af9429, 261117-124558-1a5727, 261117-124602-aed1e3, 261117-124605-ff8815 |
| A4 | 261117-210955-f5e031; fails without: 261117-082448-a97fae, 261117-082452-c9b8bb, 261117-082456-c44e69, 261117-110232-ddea0a, 261117-110235-4fbeb9, 261117-110238-651fe2, 261117-124609-74470b |
| A5 | 261117-210955-f5e031; fails without: 261117-082501-7bf23f, 261117-110242-e44601 |

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
