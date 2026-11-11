# docs-site-refresh-reference: Rewrite the docs site's reference guide and tutorials, and map docs coverage

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status docs-site-refresh-reference`.

## Outcome

The rest of docs-site-refresh (D45, D47, D55): the HTTP API reference guide, the usage tutorials and the remaining pages describe Xapiand as it runs on strata, each true of the code at the landed commit, with their request examples run as the docs E2E, and the site page `tests/coverage.md` maps every served feature to its page and tests. Split from docs-site-refresh, whose getting-started and architecture areas landed first.

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 [artifact] Every audit row (`docs/dev/docs-site-audit.md`) for the pages docs-site-refresh did not cover, marked stale, wrong or missing, is fixed or deleted, each fixed claim citing its source in the journal: the rows checked off in the journal's table
- A2 The docs E2E passes at the landed code: the `e2e` check
- A3 No page says Xapiand stores or searches through Xapian, except the upgrade page, the changelog and history pages: a `git grep -niw xapian docs/src/content/docs` job
- A4 The whole site builds without broken internal links: a light job running the site build and its link check (the 5 reference-guide pages listed on 11-10 included)
- A5 Every feature the audit lists as undocumented that docs-site-refresh did not cover (FLUSH, QUIT, WAL; search parameters; limits; the queryparser grammar) has a page section with `rest` requests and `pm.test` assertions (D47), and the request count never falls: the `e2e` check and `docs_to_postman.py` as a job before and after
- A6 [artifact] `docs/src/content/docs/tests/coverage.md` (a site page) maps each served feature to its page and its tests, and lists what is still uncovered with the reason: `docs/src/content/docs/tests/coverage.md`: "## Coverage"
- A7 The sidebar drops `next` with its page: the `e2e` check's site build, and a quote in the journal

## Contracts

None.

## Decisions

- Lead, 11-10: lands after write-path-fixes, so the pages describe `?version` as enforced on DELETE and PUT (rebase and update the reference pages before landing). Ignored parameters (`?spelling`, `?synonyms`, `?icase`, bucket `_missing`) are documented as ignored until their removal quest lands; the defects in hunt.md (`_not` lists, `_method`, `Allow`) are documented as they behave.

- Lead, 11-10 (D55): split from docs-site-refresh. The value-encoding pages and the About page's licence sentence stay sortable-encoding's (D52), and so does changelog.md (its line 40's crash promise is fixed by sortable-encoding, which edits the changelog; D55).

## May land early

Whole areas (one task each) once their pages pass A2 and A4 for their own pages.
