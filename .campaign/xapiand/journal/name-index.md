# name-index: journal

State: waiting on jobs 261117-082412-f109f8 261117-082356-ca5b98 261117-082356-64c350 261117-082416-4d23b7 261117-082420-521d8b and 9 mutants
Quest: .campaign/xapiand/quests/name-index.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1 built (xapiand:4fc89cd..b43d399), with the lead's rulings folded in (xapiand:6dcae4a): one item parser,
`index_pattern::parse` + `merged`, used by the walk and NameIndex; the 400 names its work ("directory entries" or
"index names"); options.md, changelog.md and api.md say so. Not compiled locally (host loaded).

Jobs at b43d399: targeted 261117-082412-f109f8 (test_index_pattern, test_node names, over_cap); build-test
261117-082356-ca5b98; e2e 261117-082356-64c350; A2 benches 261117-082416-4d23b7 (10^4, 10^5) and
261117-082420-521d8b (10^6), strict. Mutants (--fails-without, after the targeted job): no-parents
261117-082425-28cde9, keep-slash 261117-082430-ea3fab, drop-delta 261117-082434-4426d7, lose-merge
261117-082439-942a8a (A3); no-seek 261117-082444-aa8b4a (A2); no-range 261117-082448-a97fae, no-index
261117-082452-c9b8bb, no-hook 261117-082456-c44e69 (A4); top-build 261117-082501-7bf23f (A5).

## Next

1. Fix any failure among the jobs above; bench numbers into docs/design/name-index.md "Costs"; Evidence table;
   State ready for review. No new building after that (lead, 250k tokens).

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

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- R1 In memory, rebuilt by one walk at every start; no persisted table (design D1). The directory stays the one
  truth, so A5 has nothing to reconcile.
- R2 The build starts at the node's first pattern (lazy), not at boot (lead ruling: a walk at start costs every
  node with millions of indexes). Until it ends, patterns walk.
- R3 The lookup keeps `--max-search-indexes` as its bound, counting names read and seeks as visits (D7); `_all`
  over the cap stays 400. The walk gate does not apply to lookups.
- R4 Resolve adds on open as well as create, so an index copied in while running is listed once opened (D3).
