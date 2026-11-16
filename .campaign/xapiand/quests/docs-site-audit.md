# docs-site-audit: Audit the public docs site against the engine as it is

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status docs-site-audit`.

## Outcome

`docs/dev/docs-site-audit.md` lists, page by page, what the public docs site (`docs/src/content/docs/**`,
128 pages) says that is no longer true of Xapiand on strata, what it lacks, and what the rewrite should
be, so docs-site-refresh can start with a fixed scope. It changes no page (D45).

## Acceptance criteria

- A1 [artifact] Every page under `docs/src/content/docs/` has a row: its path, a verdict (current, stale, wrong, missing content, delete), and each stale or wrong claim with the code or doc it contradicts (`file:line` in Xapiand, strata or prospect): `docs/dev/docs-site-audit.md`
- A2 [artifact] A section per area the rewrite must cover (architecture, features, quick-start, installation and building, usage tutorials, configuration and options, the HTTP API reference, operations: dump, restore, upgrade, backup), each naming the pages it touches, what exists now in the code that no page documents, and the source of truth to write from: `docs/dev/docs-site-audit.md`: "## Rewrite scope"
- A3 [artifact] A list of every user-visible claim the audit could not verify from code, each with the probe that would settle it (a request against a running node, a test name): `docs/dev/docs-site-audit.md`: "## Unverified"
- A4 [artifact] The audit marks which pages the open quests already change (docs-upgrade-page, sortable-encoding, more-like-this, barrier-warning, release) so the rewrite does not race them: `docs/dev/docs-site-audit.md`: "## Owned by other quests"

## Contracts

None. Read-only over the site; the file it writes is its own.

## Decisions

- Owner, D45: the site is refreshed as a whole: this audit, then docs-site-refresh after sortable-encoding, then a refresh at each milestone close.

## May land early

Nothing; one landing.
