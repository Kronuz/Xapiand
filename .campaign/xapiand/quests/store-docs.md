# store-docs: Document _store (index without keeping the value) with E2E blocks

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status store-docs`.

## Outcome

The schema docs have a page (or a section of `meta-data.md`) for `_store`: a field indexed and searchable whose value is not kept in the stored document, how it inherits (a child stores only if its parent does: `Schema::feed_store`, src/database/schema.cc), and what a search and a GET return for such a field. Its examples are E2E blocks the docs suite runs (hunt row H6, issue #25; D47: every served feature has a page and tests).

## Acceptance criteria

- A1 [ci] The docs E2E blocks for `_store` pass in CI's Functional E2E (docs), each asserting values: the field matches a search, the GET body lacks it, a child under `_store: false` stays unstored
- A2 [artifact] The page names the inheritance rule and links from the schemas index: the page's path and a quote of the rule
- A3 [ci] CI is green on every platform

## Contracts

None: documents existing behavior. A docs block that disagrees with the code is a finding for the lead (hunt list), never a code change here.

## Decisions

Lead: document the behavior as it is on master; any surprise goes to the hunt list.

## May land early

None (one landing).

## Tasks

One documentation contributor (Sonnet, medium).
