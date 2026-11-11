# docs-site-refresh-reference: journal

State: waiting on jobs 261110-152519-7a2db9 261110-152514-a87e19
Quest: .campaign/xapiand/quests/docs-site-refresh-reference.md (the lead's; never edit it)

## Doing

Launch 2. Master merged at 8ba303b81 (write-path-fixes, Xapiand:0aa12bd8d). Pages changed since: delete-api, versioning,
index-api, update-api (`?version` on every write, 409 `_version`, ids 0 and "0", PATCH 204, routing by key); next.md
dropped (A7); tests/coverage.md (A6); tutorials/home and the sidebar's Tutorials group; tests.md, the new
tests/schema/reserved-subfield.md, ignore-comments, wildcard, namespace; create-api (strict needs both settings);
assertions on every request of the 12 reference pages with none (document and index exists, index delete, close-api,
boolean, keyword, text, uuid, date, geospatial, foreign-schemas, index-api) and welcome.

## Next

1. Master merged again at a637c2af0 (ci.yml only); head b5de25700 (coverage.md rewording for A3). `campaign wait` the jobs at b5de25700: e2e 261110-152519-7a2db9,
   link check 261110-152514-a87e19, request count 261110-152514-91fbb0, xapian grep 261110-152514-d0f0db.

## Blocked on

Code questions (documented as the code behaves today; the lead rules whether the code or the page changes):

1. Resolved by write-path-fixes: `DELETE` checks `?version`; delete-api.md and versioning.md say so, with 409 requests.
2. `_not` over a list of two or more subqueries is `match_all AND_NOT (A AND_NOT B)` (`src/query_dsl.cc:287-294`): probed
   on the bank corpus, `[female, brown eyes]` gives 698 (males plus brown-eyed females), not the 366 of "none of them".
   logical-operators.md says what it does.
3. `HEAD` of a missing document answers `404` with a JSON body (`document_exists_view`, `src/server/search_views.cc:813`): newman
   cannot parse it, so exists-api.md has the 404 in prose and no request.
4. `_method` in a script is always empty (`src/database/handler.cc:300`, a `TODO`); scripting.md says so.
5. `OPTIONS` answers `Allow: GET, POST, PUT, PATCH, UPDATE, STORE, DELETE, HEAD, OPTIONS` (`src/server/search_views.cc:329`): `STORE` is
   no route and the other verbs are missing. api.md asserts only that `Allow` includes `GET`.
6. `?spelling`, `?synonyms` and `?icase` are parsed and never read (`src/server/search_views.cc:2406-2450`; no use in `src/`); search-api.md says
   "read and ignored". `_missing` in a bucket aggregation is ignored the same way.
7. The variance of one value divides by zero (`src/aggregations/metrics.h`, `n - 1`) and range-aggregation open ends of an `integer` field
   are narrowed into 64-bit pairs (`bucket.h`); both pages hedge. No probe was run (audit "Unverified").
9. Ruled by the lead: kept. A strict new index must name `number_of_replicas` as well as `number_of_shards` (`src/index_resolver_lru.cc:568-588`), though
   replicas do nothing on one node (audit, namespace.md row: a decision for the owner). create-api.md states the requirement;
   namespace.md keeps its six `number_of_replicas: 1`. Options: keep it, or stop requiring replicas in strict mode (code).
8. Outside my file set: `copyright.md:19` says `src/sortable_serialise` derives from Xapian (A3 is clean for every page but this, the
   changelog and the upgrade page; sortable-encoding removes it). `src/xapian_data_guard.h:104` links an upgrade anchor that
   `upgrade.md` no longer has (`#upgrading-from-100-alpha9` against `#upgrading-from-100-alpha9-and-100-alpha10`).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `.campaign/xapiand/journal/docs-site-refresh-reference.md` "A1: audit row, fix, source" (every row of `docs/dev/docs-site-audit.md` for these pages, with its fix and source; rows owned elsewhere under "Left alone") |
| A2 | job 261110-184734-cbd2f8 (`campaign verify`, smoke, e2e and tls green at d148eac88); at the merged head 261110-184734-cbd2f8 |
| A3 | job 261110-184756-b789af at the head: the only hit is `copyright.md:19` (sortable-encoding's) |
| A4 | job 261110-184755-198651 at the head: site build + link check over every page |
| A5 | job 261110-184756-a27f76 at the head: `requests 487` (425 on the base at 2be5c57d7) |
| A6 | `docs/src/content/docs/tests/coverage.md`: "## Coverage" (feature, page, tests) and "## Not covered by a request" |
| A7 | job 261110-184734-cbd2f8 (its site build, at d148eac88). Quote, `docs/src/lib/sidebar.mjs` Quick-start items end `{ "slug": "exploration" } ]`; `next.md` deleted |

### A1: audit row, fix, source

| Page, audit row | Fix | Source |
|---|---|---|
| api.md: DELETE of an index "deletes" | `501`, linked to Delete Index | search_views.cc:1479-1491 |
| api.md: DELETE / answers 400 | `405` (PUT /, POST /, a search stay 400) | search_views.cc:1485-1488 |
| api.md: method override works on POST only | POST or GET; other methods 400; mapping likewise | search_views.cc:432-444, 619-640 |
| api.md: "most APIs support multiple indexes" | only SEARCH and COUNT; the rest 400 | search_views.cc:839,1087,1231,1290,1393,1575,1610,1759,1794 |
| api.md: "alias APIs", the `*` history sentence | removed | no "alias" in the route table |
| api.md: verbs, statuses, limits, admin commands | new sections `The Verbs`, `Administrative Commands`, `Statuses`, `Limits` | search_views.cc:187-218,548-602,652-672,1497-1535; http_utils.h:48-135; search_service.h:50; utils.h:143; query.h:94-98; aggregations.h:39-40; index_pattern.h:126 |
| documents.md: omits Scripting, Versioning | both listed | pages exist |
| delete-api.md: version precondition, index delete, "get" typo | version not checked; index delete 501; typo fixed; 404 stated | shard.cc:1448-1462; search_views.cc:833-851,1479-1491 |
| get-api.md: `#docid`, `#shard` in the default body; "primary shard" | only with `comments`; `volatile` opens writable | search_views.cc:1834-1889, 2371-2380; opts.cc:252-255 |
| index-api.md: echoed body as the default, integer id, templates, copy-pasted delete paragraph | 204 without echo, 201 + Location for POST; id by the type of `_id` (first document decides: uuid v6, integer, string); templates removed; version paragraph | search_views.cc:938-985; schema.cc:2713-2870; opts.cc:235-238; probe: fresh index mints `~...`, after `7` an integer |
| info-api.md (document): lacks version, shard, endpoint, raw_data | list completed | handler.cc:1915-1985 |
| scripting.md: query scoring, sol2, `_method`, expiry | writes only; C API; `_method` empty; foreign scripts revalidate after 1 s | lua/processor.h:50-62,100-107; handler.cc:300; lua-bounds.md item 7 |
| store-api.md: says nothing of large bodies | "Large Blobs" (16 MiB buffered, streamed blobs to 4 GiB, 413) | search_service.h:50; search_views.cc:3094-3106,3180-3185; pool.h:157 |
| update-api.md: "delete", `upsert` query param, `_version` 12, echoed bodies, PATCH missing | updates; UPSERT is a verb; `_version` 2; 204 and echo; "Patching and upserting" with requests; version paragraph | search_views.cc:575-579,1000-1060; handler.cc:603-660,1087-1102 |
| versioning.md: "older versions are ignored" | compare-and-set, last write wins without version; version 0 is create-only; 409 requests | handler.cc:339-342; strata/shard.cc:604-611,626-629; probe |
| indexes.md: omits Backups | listed by an earlier edit; checked | page |
| backups.md: NDJSON dump, RESTORE answer and limits | DUMP index and document requests; RESTORE answer with counts and request; "What a restore does" | search_views.cc:1620-1625,1643-1750,673-682,3094-3125; data.h:99-102 |
| create-api.md: name limits (243 bytes, forbidden characters) | what `index_path::valid` and the registry keyword refuse; a backslash request (400) | index_path.h:36-76; endpoint.cc:237-249; probe (238 bytes accepted, 240 refused) |
| indexes/exists-api.md, get-api.md: "one or more" | one index; a list is 400 (request) | search_views.cc:1288-1291,1385-1388 |
| indexes/info-api.md: `revision` is a document id, typo | the commit revision | handler.cc:2005 |
| indexes/delete-api.md: future-tense warning | `501`; `DELETE /` 405 | search_views.cc:1479-1491 |
| wal.md: recovery on startup, spelling and synonym entries, restore bypass | lazy replay at the first writable open; entry table; HTTP RESTORE bypass; `WAL` section with requests | shard.cc:453-502; wal.cc:324-400; search_views.cc:1663,1752-1785 |
| schemas.md: namespaces link, legacy `string` | link fixed; type names and aliases | schema.cc:880-960 |
| namespaces.md: "shallower path matches" | truncated path does not match; partial paths skip levels; depth 10 | schema.cc:4516-4560; schema.h:123 |
| field-types.md: two broken anchors, UUID level, aliases, `date` | anchors `time-datatype`, `time-delta-datatype`; UUID heading; aliases; script, array, foreign | date-type.md headings; schema.cc:880-960 |
| accuracy.md: no geo, terms-only exactness | geospatial section, defaults per type, terms-only ranges | schema.cc:161-223,4684-4694; numeric-range-terms.md |
| dynamic-field-names.md: UUID names | section | fields.h:32; schema.cc:5896-5900 |
| foreign-schemas.md: `string` id, `hedlines` | keyword; typo | schema.cc:9120-9126 |
| dynamic-typing.md: bare `*_detection` names, numeric default, numeric strings, `datetime_detection:false` outcome, detection list and order, anchors | the flags are `_`-prefixed reserved words (a bare name is a field: probed); numeric detection is on and reads JSON numbers only; a string is tried against nine patterns in order; a request asserts each detected type; `#text-detection`, `#uuid-detection` exist | schema.cc:5090-5260,1885-1910; reserved/schema.h:65-74; probe |
| indexing-mode.md: aliases | `field`, `global`, comma pairs | schema.cc:518-690 |
| boolean-type.md: strings need `_type`; `_bool_term` | `"true"`/`"false"` detect; row added | schema.cc:5193-5202,5301 |
| date-type.md L56 | six default levels | schema.cc:170-177 |
| keyword-type.md: 245-byte term | section | schema.cc:5301-5306 |
| text-type.md: undocumented parameters, `_language` default | five rows added; no default language | reserved/schema.h:30-35; schema.cc:6418-6460 |
| geospatial-type.md: `geo_point`, `2.72km`, DMS/DD "formats that work", lat/lon tip, `MULTIPCIRCLE`, x/y/z table, EWKT list | type name; integer distances; EWKT and arrays only; longitude first; spelling; table headed x, y, z; two more shapes and SRID | schema.cc:928-934; geospatial.cc:84-109; ewkt.cc:45-50,86 |
| htm.md: levels to 30/31 | 25 | schema.cc:4684-4694 |
| uuid-type.md: v1/3/4/5, condensed v1, default ids | format check, v6 default, which indexes get a uuid `_id` | schema.cc:2713-2717; opts.cc:235-238 |
| search.md: typo | fixed | |
| search-api.md: no parameters, answer | "The answer", "Parameters", five requests | search_views.cc:2340-2470,1932-2090; handler.cc:1140-1230 |
| aggregations.md: "no hard limit", "every node", "four families", `double` | 16 levels, 65,535 buckets; one node; two families; accumulators; name rule | aggregations.h:39-40; aggregations.cc:121-123; metrics.h |
| bucket.md: "not hard limited", keyed snippet, `_min_doc_count` wording, sort forms, `_missing` | limits; keys `"0"`, 12 buckets summing to 1000; "or more"; forms listed; `_missing` ignored | bucket.h; histogram-aggregation.md:83-84 |
| metrics.md: scripts, field types | no script form; accepted types | metrics.cc; metrics.h |
| filter, values, terms, histogram, range aggregation pages | multi-key OR; unsorted sample fixed; "range" intros; terms vs values; string interval; interval check; field types; half-open ranges, key rules | bucket.cc:35-80; bucket.h |
| avg, variance, std deviation, median, mode, count pages | leftover text; sample variance and deviation; median and mode structure and cost; typo; "city values" | metrics.h |
| extended_stats, stats, stop-words broken links | absolute links | link check job 261110-043755-c5fb2d |
| query-dsl.md: "match, term" | actual leaf clauses; limits | query_dsl.cc:230-280; query.h:94-98 |
| compound-queries.md: hidden `_max`, `_synonym`, `_scale_weight`, `_wildcard` | "Modifiers" | query_dsl.cc:335-340; engine/query.cc:101-111 |
| logical-operators.md: `_not`, `_xor` | text and weights | query_dsl.cc:287-294; strata/search.cc:67 |
| range-searches.md: open ends, terms-only | section | range.cc:362-364,412-428 |
| casting-values.md: `_date`, `_time`, `_geospatial`, 2221.46 | cast word list; request showing `_date` is 400 | query_dsl.cc:255-386; cast.cc:156-164 |
| simple-queries.md: weight, rank, percent always | only with `comments` | search_views.cc:2036-2047 |
| text-queries.md: no grammar | "The query string grammar" with five requests | queryparser README; query_dsl.cc:805-848 |
| default-operator.md: other modifiers | section | query_dsl.cc:805-823 |
| fuzzy.md: `_max_expansions` 50 | uncapped; `~` read at the end only | query_dsl.cc:73,846; expand.cc:121-124 |
| love-and-hate.md, near.md, phrase.md | lone exclusion 400; window 10 + n - 1, plain words only; no wildcard in a phrase | queryparser README; parse-ab.jsonl |
| partial.md: `wildc*` is a partial | `**` or `_partial`; 50-term cap | query_dsl.cc:72,756-758 |
| stop-words.md: `few`, `far`, `between`; `_stopwords`; list needs `_language` | only `days` is searched; `_stopwords` is 400; list needs a language | stopwords/en.txt; stopper.cc:85-92 |
| wildcard.md: `?`, no cap | `?` rules; uncapped | query_dsl.cc:71,733-739,775-790; expand.cc:70-100 |

| delete-api.md, versioning.md, index-api.md, update-api.md, api.md (write-path-fixes) | `?version` on PUT, PATCH, UPDATE, UPSERT, DELETE; 409 `_version`; version 0 semantics; ids 0 and "0"; routing by key; PATCH 204; POST with echo 200 + Location | docs/mechanisms/document-ids-and-versions.md; tests/test_node.cc:1309-1470; search_views.cc write_document_view, on_error |
| tutorials/home.md: missing content | index of the tutorials; spatial-search in the sidebar | pages |
| tests.md: 15 of 21 pages, stray request, "Replace Value by Null" | every page listed; request moved to tests/schema/reserved-subfield.md; title | sidebar.mjs; schema.cc ("is not valid") |
| tests/comments/ignore-comments.md: vacuous assertion | GET the index, walk `_schema` keys, must be non-empty | search_views.cc write_database_view (echo = retrieve_database) |
| tests/querydsl/wildcard.md: keyword half asserts nothing, typo | `field:dynamic*` asserts the hit; typo | query_dsl.cc:710-731 |
| tests/namespace/namespace.md: js outside e2e, unasserted PUT, reused heading, dead helper | fixed; `number_of_replicas` kept (Blocked on 9) | index_resolver_lru.cc:568-588 |
| stem-language.md, basque-language.md: orphans | in the sidebar | |
| next.md: delete | deleted, sidebar entry dropped | |
| close-api.md: future-tense feature text | `CLOSE`/`OPEN` answer 501, asserted | search_views.cc:656-672 |

### Left alone (not mine, or no source)

- numeric-type.md, the accuracy paragraphs of date-type.md and geospatial-type.md, the "stored as seconds" line of date-type.md: how values are encoded is sortable-encoding's.
- info-api.md (document) terms, search-api.md `?nearest`/`?fuzzy`, query-dsl.md `more_like_this`: more-like-this owns them (not documented as absent or present).
- api.md root commands (`HEAD /:stats`, `/:schema`): gate-prep-followups; wal.md `:wal` streams: gate-prep-followups A5.
- store-api.md Lenna examples: license-closure; text-type.md CJK note: cjk-words.
- store-api.md `UPDATE` with `Content-Length: 0`, bucket.md "Mixing field types" warning, explicit-types.md negative `positive`: audit "Unverified", no probe settled them; text left as is.
- near.md and phrase.md say "words only": phrase-expansion extends that.
- benchmarks.md: closing-benchmark's. conduct.md, astro.config.mjs, docs/README.md: outside the file set. contributing.md: audit "current".
- Sidebar still lacks about, help, options, wal, htm (they run after the sidebar pages); adding them reorders the E2E, not done here.

- e2e runs that failed outside the quest: 261110-173046-0fdb94 (a RESTORE timed out at 30 s under load) and 261110-175030-6fc614 (the TLS renegotiation check); both are hunt rows.

## Result

New request blocks (52 requests, all with assertions): api.md `OPTIONS`, `LOCK` (405), `COMMIT` of a document (405) and of an index,
`CHECK`, `QUIT` and `FLUSH` (405), and the limits (33 patterns, aggregations at 17 and 16 levels, a body 201 levels deep); delete-api
(`DELETE` 204, `GET` 404, second `DELETE` 404, version ignored); get-api (`comments`, `selector`); info-api (`human`); update-api (`PATCH`,
`UPDATE` 404, `UPSERT`); versioning (version 0 twice, version 1, stale version); backups (`DUMP` of an index and a document, `RESTORE`
answer, the restored document); create-api (a backslash name, 400); get index (a list, 400); wal.md (`WAL` of a one-shard index, of a
five-shard one); dynamic-typing (detected types, datetime detection off); search-api (limit, offset, sort; two sorts; two `q`; selector;
`COUNT`); text-queries (OR, `_and`, `_and` with NOT, `+`/`-`, AND as a word); casting-values (`_date` 400). New indexes: `deleteapi`, `updateapi`,
`versions`, `walapi`, `backups/tweet`, `dyn_typing`, `dyn_datetime`, `limits`.

For docs-coverage.md (A6, next launch). Covered by a request in the reference guide: OPTIONS, LOCK-style 405, COMMIT, CHECK, FLUSH and QUIT
(405 only), WAL, DUMP, RESTORE, PATCH, UPSERT, UPDATE, HEAD, DELETE, version conflicts, the search parameters (`limit`, `offset`, `sort`, `q`,
`selector`, `collapse`, `COUNT`), the limits (patterns, aggregation depth, body nesting), the query string grammar, cast words, detection.
Uncovered, with the reason: FLUSH and QUIT with `--admin-commands` (the harness node runs without it; QUIT would stop it); the 16 MiB body (413) and
streamed blobs (too large for a docs request); 4,096 clauses and 100 levels of a query (the request would be pages long); `Retry-After` 503/429
(needs load); `check_at_least`, `volatile`, `spelling`, `synonyms`, `icase` (no observable effect through a request); `--max-search-indexes`.

## For the next launch

1. Tutorials: `tutorials/home.md` (audit: missing content) and `tutorials/spatial-search.md` (current; license-closure A5 owns the map screenshot).
2. The tests/ pages' rows (`tests/comments/ignore-comments.md` stale, `tests.md` missing content, the two orphan fixtures, the stem/basque and
   querydsl merge candidates), `benchmarks.md` (closing-benchmark), `contributing.md` (current), `conduct.md`.
3. A6 `tests/coverage.md` from the lists above and the two earlier journals' "For docs-coverage.md" paragraphs; A7 drop `next` from
   `docs/src/lib/sidebar.mjs` (it lists `next`; getting-started kept the page for that reason) and the page; the sidebar also lacks `about`, `help`,
   `options`, `wal`, `htm`, `spatial-search`; its header says it is generated by a script that does not exist.
4. `docs/astro.config.mjs` `editLink.baseUrl` and `docs/README.md` (the audit's site-infrastructure rows) are outside this quest's file set.

## Rulings I made

1. New examples use their own index names, never `twitter/tweet/1`: the e2e runs the pages in sidebar order and `documents/delete-api.md` deletes that
   document before the index pages read `twitter/tweet`.
2. Request blocks use `?comments=true`, `?human=true` forms (the getting-started ruling); the harness always adds `commit`, `volatile` and `echo`, so
   assertions accept the echoed answer, and the prose states `204`/`201` for a plain request.
3. The `*_detection` names in dynamic-typing.md are `_`-prefixed: a bare name is an ordinary field (probe: `datetime_detection: false` left a
   datetime detected). The audit did not catch it; the page's two old requests were meaningless.
4. Where the code looks unintended (Blocked on 1, 2, 4, 6) the page states what the code does, with its source, and nothing is promised for the future.
5. A parameter or limit is stated only when a line of code or a probe on the Nov 3 binary settled it; a claim the audit marked unverified is left out or
   hedged (range open ends, variance of one value).
