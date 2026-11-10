# docs-site-refresh--getting-started: journal

State: waiting on jobs 261110-034137-86b602 261110-034138-4e0945 261110-034138-293b85 261110-034139-bc57c3
Quest: .campaign/xapiand/quests/docs-site-refresh.md (the lead's; never edit it)

## Doing

All 13 pages are written and committed (head 6427559ac). The jobs below prove them. The 56 requests of the 6
pages that carry them also ran against a Nov 3 build of the node (GREEN, 77 of 77 assertions;
`~/.scratch/gs-probe/run.sh`); that binary is older than the base, so the job is the proof.

## Next

1. `campaign wait` the jobs; fix what the e2e or the link check reports (a failing assertion names its request).
2. Record the results in Evidence, set State to ready for review.

## Blocked on

Nothing. Two things outside my file set for the lead:

1. `docs/src/lib/sidebar.mjs` lists `next`, so `next.md` stays (rewritten as a real page of links) instead of
   being deleted as the audit proposes; drop the sidebar entry first, then the page. `options`, `about` and
   `help` are in no sidebar (the audit's site-infrastructure row).
2. Code question: `HEAD` of a missing document answers `404` with a JSON body (`document_exists_view`,
   src/server/search_views.cc:813); an HTTP client such as newman fails to parse the response. manipulation.md
   documents the 404 and carries no request for it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | the table below: every audit row of the 13 pages |
| A2 | job 261110-034137-86b602 (the quest's e2e check at 6427559ac; pending) |
| A3 | `git grep -niw xapian` over the 13 pages: no output at 6427559ac |
| A4 | job 261110-034138-4e0945 (npm ci, astro build, `~/.scratch/gs-probe/linkcheck.py` over the 13 pages; pending) |
| A5 | request count 396 at d02dd743d (job 261110-034139-bc57c3) and 425 at 6427559ac (job 261110-034138-293b85); new blocks under Result; the options list is options.md |
| A6 | the lead's `docs/dev/docs-coverage.md`; the rows for my pages are under Result |
| A7 | manual, the owner |

### A1: audit row, fix, source

| Page, audit row | Fix | Source |
|---|---|---|
| index.mdx: Benchmarks and Memory Usage links to Xapian-era numbers | cards link Architecture and Options | benchmarks.md is closing-benchmark's |
| index.mdx: "Multi-Types" | "Multi-Tenant indexes that nest" | indexes nest: changelog.md index-patterns entry; src/server/search_views.cc:187-600 |
| index.mdx: "Low Memory Usage" (unmeasured) | one process, no separate services | CMakeLists.txt (strata, prospect linked in-process) |
| about.md: lacks single node, TLS, Lua bounds, wide search; "Multi Types" | feature list rewritten | src/opts.cc:154-183; changelog.md (Lua bounds, wide search, `:metrics`); search_views.cc:536-545 |
| about.md: MIT claim | left unchanged | src/sortable_serialise.cc still in tree (sortable-encoding owns it) |
| welcome.md: "Highly Available Distributed", "deploying to various environments" | single node, strata; "configuring and stopping" | changelog.md Removed (cluster); docs/status/behavior-changes-audit.md #3 |
| welcome.md: examples are echoed | note on echo and `-vvvv` | src/server/search_views.cc:938-985, 2190-2237 |
| help.md: freenode IRC, Twitter unverified | both removed; "deployment" to "configuration" | audit Unverified (no code probe settles it); ruling 1 |
| faq.md: no answers for data dir, ports, auth, cluster, stop, exit 65, 16 MiB | six answers added; 204/201 stated | src/opts.cc:43,263-265; src/main.cc:652-669,803; search_views.cc:1479-1491 (501), 3180-3182 (413) |
| next.md: delete | rewritten as a page of links (the sidebar lists it) | docs/src/lib/sidebar.mjs |
| basics.md: NRT "a couple seconds" | "Visibility and Durability": read-your-writes, commit 1 s / 3 s / 8-10 s | docs/mechanisms/reader-views.md:24; src/opts.cc:64-69; probe: PUT then SEARCH finds it at once |
| basics.md: index path, shards | nested path, `.__N` shard names, `number_of_shards` blocks | src/index_resolver_lru.cc:191; INFO shards (src/database/handler.cc:2022-2062) |
| installation.md: Docker variables and volume | table of XAPIAND_DATABASE/NAME/UID/GID | contrib/docker/xapiand/entrypoint.sh:33-36,96-104; Dockerfile:80-87,92 |
| building.md: CMake minimum | 3.14 in practice, 3.12 declared | CMakeLists.txt:26,747 (FetchContent_MakeAvailable) |
| building.md: OpenSSL 3, libuuid, zstd missing | requirements list | CMakeLists.txt:375,388,404,960-966; Dockerfile:34-46 |
| building.md: option table incomplete | 24 options, development ones apart | CMakeLists.txt:46-70; cmake/Modules/FindSanitizers.cmake:32-35 |
| building.md: `ninja check` | needs BUILD_TESTS=ON | CMakeLists.txt:1857-1860 |
| building.md: "Clang >= 14 / GCC >= 13" unverified | replaced by what CI builds with | .github/workflows/ci.yml:18; Dockerfile:36; bottles.yml (Apple Clang) |
| options.md: 29 options on no page | every option of opts.cc documented | src/opts.cc:110-272, 300-410 |
| options.md: removed options | "Removed Options" | changelog.md Removed; harness/removed_options_check.sh |
| options.md: environment | XAPIAND_DATA_PATH, XAPIAND_LAG_PROBE, build-conditional ones | src/main.cc:932-990; src/manager.cc:113-117; CMakeLists.txt:70 |
| options.md: `--admin-commands` | section, with the 405 | search_views.cc:1497-1535 |
| options.md: `:metrics` | Monitoring section | search_views.cc:523-529, 800-808 |
| quickstart.md: "the twitter index" | `twitter/user` and `twitter/tweet` named | search_views.cc route_key; probe INFO endpoint |
| quickstart.md: no data-directory or port note | "The node" section | src/opts.cc:263-265; config.h.in:302 |
| quickstart.md: echo not mentioned | "What a write answers" | search_views.cc:938-985 (204, 201 + Location, 200 echo) |
| manipulation.md: "the user index"; one-second delay | `twitter/user`; read-your-writes and commit times | reader-views.md:24; opts.cc:64-69 |
| exploration.md: `reference-guide/query-dsl` link | `/Xapiand/reference-guide/search/query-dsl` | page path |
| exploration.md: `q=*` text, `#` fields, took, aggregations | rewritten | search_views.cc:1995-2063 |
| exploration.md: `exploration#sample-dataset` broken from 16 pages | anchor `sample-dataset` added | docs-site-audit.md site infrastructure |

## Result

Pages: index.mdx, welcome.md, about.md, help.md, faq.md, next.md, quickstart.md, basics.md, manipulation.md,
exploration.md, installation.md, building.md, options.md.

New request blocks (29 requests, with assertions): quickstart `GET /`, `PUT` 204, `POST` 201 + Location, `COUNT`;
basics `PUT` with `number_of_shards`, two `INFO` shard lists, a default-shards `PUT`; manipulation `PATCH` (and
its setup `PUT`), two `UPSERT`, `DELETE` of an index (501), `HEAD`, `COMMIT`, `CHECK`; exploration `?comments`,
`COUNT`, a two-field `_sort`; options `?pretty` on and off, `?human` on and off, `?comments`, a strict index
(412), `FLUSH` without `--admin-commands` (405), `GET /:metrics`, and their setup writes.

For docs-coverage.md. Covered by these pages: PUT, POST, UPDATE, PATCH, UPSERT, DELETE, HEAD, COMMIT, CHECK,
COUNT, SEARCH, INFO, RESTORE, FLUSH (the 405), `GET /` and `GET /:metrics`; the options echo, pretty, human,
comments, strict and shards. Uncovered by a request, with the reason: the startup-only options (`--port`, `-D`,
`--name`, `--processors`, the caches, `--uid`, `--gid`, `--detach`, `--log`, `--color`, `--iterm2`, `--uuid`,
`--legacy-ids`; the harness starts the node, and TLS runs the whole suite through the `tls` layer); `QUIT`
(would stop the node); `WAL` and the search URL parameters (reference-guide pages, not these).

## Rulings I made

1. `help.md` drops the freenode IRC channel and the Twitter account: the audit could not confirm either; GitHub
   Issues remains. Restore them if the owner confirms them.
2. `next.md` is kept as a page of links (the sidebar lists it); the audit's delete waits for the sidebar.
3. Request blocks use `?pretty=true`, `?human=true`, `?comments=true`, not the bare flags: the collection
   generator drops valueless parameters, so a bare flag is never sent. The prose names the bare forms.
4. The about.md MIT sentence is unchanged until sortable-encoding removes the GPL files.
5. The barrier-warning switch is not documented (that quest has not landed).
6. building.md states the CMake minimum as 3.14 in practice, with the declared 3.12, because the build calls
   `FetchContent_MakeAvailable`.
