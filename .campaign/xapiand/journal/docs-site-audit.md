# docs-site-audit: journal

State: ready for review
Quest: .campaign/xapiand/quests/docs-site-audit.md (the lead's; never edit it)

## Doing

Done. `docs/dev/docs-site-audit.md` written (128 pages: current 45, missing content 34, stale 28, wrong 20, delete 1).
Method: five fresh readers each took a page group (documents/indexes/api/wal; schemas; search and aggregations; query DSL; tests) and
verified claims against `src/` with file:line; I took the 24 top-level pages and merged. I spot-checked the load-bearing findings
against the code (DELETE index 501 `search_views.cc:1479-1491`, `numeric_detection` default `schema.cc:1899`, fuzzy expansion
default 0 `query_dsl.cc:73`). No page changed, no job run (no probe settles an item cheaply without a node).

## Next

1. Lead reviews; docs-site-refresh starts from "## Rewrite scope".

## Blocked on

Docs and code disagree where the code may be the thing to change (details and file:line in the audit, "## Code questions the audit raised"):
1. `DELETE /index/` answers 501 (`src/server/search_views.cc:1479-1491`); `docs/src/content/docs/reference-guide/api.md:156-167` says it deletes the index. Owner call: implement it or document the gap.
2. A strict new index requires `number_of_replicas` (`src/index_resolver_lru.cc:573-585`) though the cluster is gone; `reference-guide/indexes/create-api.md:55` says it has no effect.
3. Range aggregation open bounds on integer fields (`src/aggregations/bucket.h` `ranges_i64`) may drop negatives; `_not` over 2+ subqueries (`src/query_dsl.cc:287-294`) is not "none of them" (`logical-operators.md:15`); `DELETE ?version` is ignored (`search_views.cc:833-851`, `documents/delete-api.md:32-37` promises 409). Each has a probe under "## Unverified".
4. `src/xapian_data_guard.h:104` links `/Xapiand/upgrade/#upgrading-from-100-alpha9`; `docs-upgrade-page` writes `upgrading.md`: keep `upgrade.md` and its anchor or change the constant together.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/dev/docs-site-audit.md`: "## Page table" |
| A2 | `docs/dev/docs-site-audit.md`: "## Rewrite scope" |
| A3 | `docs/dev/docs-site-audit.md`: "## Unverified" |
| A4 | `docs/dev/docs-site-audit.md`: "## Owned by other quests" |

## Result

`docs/dev/docs-site-audit.md`. Counts: 128 pages = current 45, missing content 34, stale 28, wrong 20, delete 1 (`next.md`).
Biggest gaps: no page documents a removed feature as live (the Xapian purge is done) except `welcome.md` ("Highly Available
Distributed"), `benchmarks.md` (Xapian-era figures) and `architecture/internals.md` (ten nonexistent paths); the heavier defect is
old pages never true of this code (20 wrong) and behaviors with no page: verbs COMMIT/FLUSH/QUIT/CHECK/UPSERT/PATCH/WAL, 29 of 53
options, the search URL parameters and response, query/aggregation/body limits, the queryparser grammar. 38 unverified claims with
probes; 11 open quests mapped. Site infrastructure: sidebar generator missing, `editLink` points at `docs-astro/`, 8 pages in no
sidebar, 16+ broken anchors.

## Rulings I made

- Used subagent readers per page group (judgment work in parallel); their rows are merged unedited except the options counts, which I recomputed from `src/opts.cc` (53 options, 29 on no page).
- A page that is both stale and missing content takes the worse verdict; `current` is allowed with a noted weak test (tests/restore).
- Docs/code disagreements go in "## Code questions the audit raised" and above under Blocked on; the audit does not rule on them.
