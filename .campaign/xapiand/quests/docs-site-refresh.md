# docs-site-refresh: Rewrite the public docs site: architecture, features, quick-start, usage, configuration, reference

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status docs-site-refresh`.

## Outcome

The public docs site describes Xapiand as it runs on strata: architecture, features, quick-start,
installation and building, usage tutorials, configuration and options, the HTTP API reference and
operations, each true of the code at the landed commit, with no page still describing Xapian
internals as Xapiand's (D45). The scope is `docs/dev/docs-site-audit.md`'s "Rewrite scope", minus
pages another open quest owns. The lead may split it into one task per area.

## Acceptance criteria

- A1 [artifact] Every row of the audit marked stale, wrong or missing content is fixed or deleted, and each fixed claim cites its source in the journal: `docs/dev/docs-site-audit.md` rows checked off in the journal's table
- A2 The docs E2E (every request example the site runs) passes at the landed code: the `e2e` check
- A3 No page says Xapiand stores or searches through Xapian, except the upgrade page, the changelog and history pages: a `git grep -niw xapian docs/src/content/docs` job whose output lists only those
- A4 The site builds without broken internal links: a light job running the site build and its link check
- A5 [manual] The owner reads the architecture, quick-start and configuration pages before landing

## Contracts

None.

## Decisions

- Owner, D45. After sortable-encoding, so the value-encoding pages are written once.

## May land early

Whole areas (one task each) once their pages pass A2 and A4.
