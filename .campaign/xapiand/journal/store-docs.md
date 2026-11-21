# store-docs: journal

State: ready for CI: pushed; CI Functional E2E (docs) decides A1, A3
Quest: .campaign/xapiand/quests/store-docs.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Page written and pushed; no local job (a full build for 7 blocks; CI's Functional E2E (docs) runs them).

## Next

1. Read CI at the pushed commit; fix any block that fails (values, not code).
2. Lead: widen the file set for the two links A2 needs (see Blocked on).

## Blocked on

Lead: A2 wants the page linked "from the schemas index". The index is
docs/src/content/docs/reference-guide/schemas.md and the sidebar is docs/src/lib/sidebar.mjs, both
outside `schemas/*.md`. I linked the page from indexing-mode.md (itself in the index) instead.
Needed: a "## [Storing Mode](store)" section in schemas.md and a sidebar entry
`reference-guide/schemas/store` after indexing-mode (the page runs last in the E2E order until then,
alphabetically after sidebar pages; its blocks are self-contained so order is harmless).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | CI Functional E2E (docs) at the pushed commit (pending) |
| A2 | docs/src/content/docs/reference-guide/schemas/store.md: "a field is stored when its own `_store` is `true` **and** its parent is stored"; linked from indexing-mode.md ("see [Storing Mode](store)"); schemas.md link pending the file-set widening |
| A3 | CI (pending) |

## Result

New page schemas/store.md: what `_store` does, the inheritance rule with a truth table, one example
with 5 asserting blocks (unstored field matches a search; the hit and the GET body lack it; a
child with `_store: true` under a `_store: false` parent stays unstored and still searches).

## Rulings I made

New page instead of a meta-data.md section: `_store` is about storage, not metadata.
Blocks assert absence with `to.not.have.property` and JSON-string search, so the shape of the
empty `profile` object in the GET body is not pinned (the code at schema.cc:3493 sets it to an
empty map only when stored; not verified at runtime).

Findings for the lead: none yet; if CI shows a block failing, the code's behavior is the truth and
the block changes.
