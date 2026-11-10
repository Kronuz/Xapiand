# GitHub triage

> Status: triage of 2026-11-09 (quest `github-triage`, owner ruling "go through all issues and PRs, addressing and closing them when done"). Nothing was posted, closed or merged: the lead posts the replies and closes after landing.
> Read against: Xapiand master at `e824e9171` plus this quest's change, the libraries beside it (`~/code/Kronuz/analysis`, `reactor`, `http`) and `.campaign/xapiand/PLAN.md` Decisions.
> Not read: Xapian, `src/sortable_serialise.{cc,h}`, `contrib/python/xapiand-py/xapiand/utils/sortable.py` (GPL). No node was started: a claim only a running node settles is marked "not run".

## Summary

16 open issues, 12 Dependabot pull requests.

Verdicts: fixed or works now 5 (#2, #11, #13, #29, #39), answered as removed 3 (#24, #26, #34), answered with the current behavior 3 (#25, #27, #33), becomes new work 4 (#14, #18, #21, #35; #35 is already a hunt row), cannot verify 1 (#28).

The owner's "not planned" call (one batch, below): #24, #26, #34, the at-rest half of #25, and #28 unless the reporter retests.

## Dependabot pull requests

Applied as one commit to the declared ranges in `docs/` and `docs/discussions/worker/` (Xapiand `quest/xapiand/github-triage`). The lockfiles on master already resolve every package at or above the version of the pull request that bumps it, so the change is the two ranges each lockfile still declared lower. Landing it closes the twelve.

| PR | Package | PR version | Master lockfile resolves | What the commit does |
|---|---|---|---|---|
| #51 | postcss (docs) | 8.5.25 | 8.5.30 | nothing: already newer |
| #54 | nanoid (docs) | 3.3.18 | 3.3.20 | nothing: already newer |
| #55 | svgo (docs) | 4.1.0 | 4.1.0 | nothing: already there |
| #56 | hono (worker) | 4.13.5 | 4.13.13 | range `^4.6.14` to `^4.13.5` |
| #57 | astro (docs) | 7.3.2 | 7.4.0 | range `^7.0.6` to `^7.3.2` |
| #58 | sharp (docs) | 0.35.4 | 0.35.5 | nothing: `package.json` already `^0.35.5` |
| #59 | sharp, wrangler (worker) | 0.35.4, wrangler 4.131.0 | 0.35.5, 4.157.0 | wrangler range raised by #64's value |
| #60 | smol-toml (docs) | 1.8.0 | 1.9.1 | nothing: already newer |
| #61 | js-yaml (docs) | 4.3.2 | 4.3.2 | range `^4.1.0` to `^4.3.2` |
| #64 | undici, wrangler (worker) | undici 7.29.0 (via #59), wrangler 4.145.0 | undici 7.30.1, wrangler 4.157.0 | range `^4.110.0` to `^4.145.0` |
| #65 | devalue (docs) | 5.9.4 | 6.0.3 | nothing: already newer |
| #66 | source-map-js (docs) | 1.2.2 | 1.2.2 | nothing: already there |

The jobs that prove it (site build, worker tests, `npm audit` before and after) are in the quest journal.

## Issues

| # | Title | Verdict | Detent |
|---|---|---|---|
| 2 | Documentation Status | works now: the site is audited and being rewritten | no |
| 11 | Add Xapiand as a Debian package | works now: a `.deb` ships with every release | yes (packaging) |
| 13 | Update README.md with usage guide | works now: the README has install and quick start | no |
| 14 | N-Gram index/search example | new work: n-gram terms for text (hunt row H1); typos are served by `word~` | no |
| 18 | Limit query execution time | new work: a per-search time limit (hunt row H2) | yes (query deadlines) |
| 21 | Result Highlighting | new work: highlighting (hunt row H3, owner scope call) | no |
| 24 | Xapaind Glossary Terms | removed: the cluster is gone from this release; not planned (owner) | yes (glossary) |
| 25 | Data Encryption | answered: TLS in transit, volume encryption at rest; at rest not planned (owner) | yes (encryption at rest) |
| 26 | How to config a cluster without multicast? | removed: discovery is gone; not planned (owner) | yes (seed-list discovery) |
| 27 | Write performance problem, can not reach 1000qps | answered: the queue in the profile is deleted; the benchmark is pending | yes (write path) |
| 28 | Google Cloud Run | cannot verify here; not planned unless retested (owner) | yes (sandboxed runtimes) |
| 29 | compile with cmake : linking (error) | fixed (Xapiand:009cfac58, 2020) | no |
| 33 | License question | answered, owner to review the wording | yes (notices) |
| 34 | upgrade to Xapian 1.4.16 | removed: master has no Xapian; not planned (owner) | no |
| 35 | DELETE all  documents | new work: index deletion (hunt row already filed) | yes (replicated drop) |
| 39 | Is this project dead? | works now: it is active | no |

Each reply is under 120 words. "Cites" lists the `file:line` or docs page behind each claim about current behavior.

### #2 Documentation Status

Verdict: works now. The docs site has 128 pages, was audited page by page on 2026-11-09, and its whole-site rewrite is decided (PLAN.md D45, D47).

Reply:

> The documentation now lives at https://kronuz.github.io/Xapiand/ and has been rewritten around the engine as it is today. We audited all 128 pages against the code; the pages that are still stale or incomplete are listed in docs/dev/docs-site-audit.md in the repository, and a full rewrite of the site is planned. The request examples on the pages run as end-to-end tests. Corrections and new pages are welcome as pull requests; see the contributing guide on the site. Closing this as the original question is answered.

Cites: `docs/dev/docs-site-audit.md` "Summary" (128 pages, 45 current), `.campaign/xapiand/PLAN.md` D45 and D47, `docs/src/content/docs/contributing.md`. Not run: the site's "Improve this page" link points at a directory that does not exist (audit "Summary" item 4); the reply does not mention it.

Detent: no.

### #11 Add Xapiand as a Debian package

Verdict: works now. A `.deb` for amd64 and arm64 is built and attached to every release.

Reply:

> Every release now carries a .deb for amd64 and arm64 (v1.0.0-alpha.9 has xapiand_1.0.0.alpha.9_amd64.deb and the arm64 one), built by the release workflow. It targets Debian 13 and Ubuntu 24.04 or newer, and installs with `sudo dpkg -i xapiand_*_amd64.deb`. RPMs, a FreeBSD package, a Homebrew formula and a Docker image (ghcr.io/kronuz/xapiand) ship the same way. Getting it into the Debian archive or an apt repository is not planned. See the Installation page.

Cites: `.github/workflows/release.yml:195-253` (deb job), `docs/src/content/docs/installation.md:35-50` (Linux), release v1.0.0-alpha.9 assets (`gh release view`). The archive and apt-repository statement is a plan statement: owner confirms.

Detent: yes. The release pipeline (fpm for rpm and deb, bottles, a multi-arch image) packages one binary; a replicated system needs the same packages plus a service unit and a data-directory layout per node.

### #13 Update README.md with usage guide

Verdict: works now. The README opens with install and a three-command quick start.

Reply:

> The README now has an Installation section (Docker image, Homebrew, and the DEB and RPM packages attached to each release), a Quick start with curl commands to index, get and search a document, and a Building section with the CMake steps and options. The Installation and Quick-start pages on the site go further. Closing as done.

Cites: `README.md:15-24` (Installation), `README.md:26-39` (Quick start), `README.md:76-98` (Building).

Detent: no.

### #14 N-Gram index/search example

Verdict: becomes a hunt row (H1). Typo tolerance works now through fuzzy terms; n-gram terms for Latin text are not generated, and the `_ngram` schema flag is accepted but unused.

Reply:

> Xapiand does not generate n-grams for ordinary text. For your example, a fuzzy term finds it: `message:storaeg~` matches `storage` within the default edit distance of 2, and `stor*` matches by prefix; see the Fuzzy and Wildcard pages under Text Queries. A `_cjk_ngram` option exists for CJK text only. The `_ngram` schema property is accepted and does nothing today; we are tracking either implementing it or removing it. Until then, send n-grams as a keyword array from the client.

Cites: `src/query_dsl.cc:74` (`DEFAULT_FUZZY_EDIT_DISTANCE 2`), `docs/src/content/docs/reference-guide/search/query-dsl/leaf-queries/text-queries/fuzzy.md` and `wildcard.md`, `docs/src/content/docs/tests/querydsl/fuzzy.md` (`unserten~3` finds `uncertain`), `src/multivalue/generate_terms.cc:521-540` (only `cjk_ngram` reaches the term generator), `src/database/schema.cc:5631,5663,5673` (the callers pass `cjk_ngram`, never `ngram`), `src/database/schema.cc:7018` (`_ngram` read into `flags.ngram`; elsewhere only echoed at `:4983` and compared in the change check at `:8510`). Not run: that `storaeg~` matches `storage` (the edit distance is Levenshtein-or-better, two substitutions; the E2E fuzzy page covers `unserten~3`).

Detent: no.

### #18 Limit query execution time

Verdict: becomes a hunt row (H2). Operators are documented and size-capped; no wall-clock limit exists, though a client that disconnects stops its search.

Reply:

> Text queries are parsed by the queryparser library; the operators are on the Text Queries pages of the site and in the library's README. A query is capped at 100 nested operator levels and 4096 terms and operators, and a larger one answers 400. There is no execution-time limit today. A search does stop at its next window boundary when the client disconnects or the service stops, so a time limit is a matter of arming that cancellation from a timer. I'm adding it to the list of work to do.

Cites: `docs/src/content/docs/reference-guide/search/query-dsl/leaf-queries/text-queries.md` (operators), `docs/src/content/docs/changelog.md:163-168` (size limits), `src/engine/search.h:91-93` (budget cancels at window boundaries), `src/server/search_views.cc:2321,3132,3200-3203` (the request's token; a cancelled request answers 503). No `timeout` option or parameter in `src/opts.cc` or `src/server/search_views.cc` (grep).

Detent: yes. A search that fans out over shards needs one deadline that every leg inherits and cancels on; the cancellation token and budget are the shared pieces (PLAN.md D20 gives task's cancellation design to Detent's lead).

### #21 Result Highlighting

Verdict: becomes a hunt row (H3), and a scope call for the owner. Never served on this code; the Xapian call named in the issue is gone with Xapian.

Reply:

> Highlighting is not supported. The issue names Xapian's MSet::snippet, and Xapiand no longer uses Xapian, so there is nothing to expose. Elasticsearch, Tantivy and Quickwit all offer it and it would fit the engine, since a field's stored text can be re-run through the same analyzers to find the matched words. It is on the list as a candidate, with no date.

Cites: no `highlight` or `snippet` in `src/` outside a JSON error excerpt (`src/database/utils.cc:214`, grep), `docs/src/content/docs/changelog.md:244-246` (strata is the only engine). The peers' support is from memory and unchecked here; D40's test for bringing a feature back is "Elasticsearch, Quickwit or peers serve it and it fits naturally" (PLAN.md D40).

Detent: no.

### #24 Xapaind Glossary Terms

Verdict: answered as removed. The cluster terms the reporter asks about (clusters, nodes, replication, leaders) describe code that is gone from this release.

Reply:

> This release runs on a single node. The cluster is removed: node discovery, replication, primary election and distributed search are gone, along with `--cluster`, `--discovery-*` and the other cluster options, so two instances do not replicate or sync. Shards remain, as the pieces one index is split into on one node. A glossary of cluster terms would describe nothing that exists, so I'm closing this as not planned.

Cites: `docs/src/content/docs/changelog.md:247-252` (cluster removed, with the option list), `src/opts.cc` (no cluster, discovery or replica options; grep), PLAN.md D9 (the single-node release has no cluster). The word "shards" as per-node pieces: `docs/src/content/docs/reference-guide/indexes/create-api.md` (`number_of_shards`) and `src/index_resolver_lru.cc:590-596`.

Owner: "not planned".

Detent: yes. A glossary (cluster, node, shard, replica, leader, term) for the replicated system, with how they relate, is what this reporter could not find; worth one page in Detent's docs.

### #25 Data Encryption

Verdict: answered. TLS in transit works now; at rest is not offered, and the volume-encryption advice stands. Indexing a field without storing it works through `_store`.

Reply:

> Traffic can be encrypted: start with `--tls-cert` and `--tls-key` to serve the API over HTTPS (Options page). Data at rest is not encrypted by Xapiand, and index terms are in the engine's files too, so the answer is still to encrypt the volume (LUKS, dm-crypt, FileVault). To index a field without keeping its value in the stored document, set `_store` to false on it. Accents are not folded: `poeme` will not match `poème`. Closing the at-rest part as not planned.

Cites: `docs/src/content/docs/options.md:108-119` (HTTPS), `src/opts.cc:200-202`, `src/database/schema.cc:7209-7216` (`feed_store`) and `:3016,3029` (a field with `store` false is not inserted into the document data), no `encrypt` in `src/` (grep). Accents: `~/code/Kronuz/analysis/README.md:10` lists case folding only and `analysis.hh` has no accent filter (grep); not run. `_store` is on no docs page (hunt row H6).

Owner: "not planned" for encryption at rest.

Detent: yes. Encryption of the data files and of the copies on every replica (key per volume, or per segment with the key outside the node) is the replicated store's version of this; the replication stream is a second place the plaintext crosses.

### #26 How to config a cluster without multicast?

Verdict: answered as removed.

Reply:

> There is no cluster in this release, with or without multicast: discovery, replication and distributed search are removed, and so are the `--cluster` and `--discovery-*` options. It runs as a single node. Closing as not planned.

Cites: `docs/src/content/docs/changelog.md:247-252`, `src/opts.cc` (no discovery options), PLAN.md D9 (cluster work parked after the release as 7b).

Owner: "not planned".

Detent: yes. Peer discovery without multicast (a static seed list, DNS names, or a join through any one live member) is a requirement for a 50-server deployment across networks; worth stating in the replicated system's bootstrap design.

### #27 Write performance problem, can not reach 1000qps

Verdict: answered. The `doc_preparer` queue in the reporter's profile no longer exists; there is no published throughput number for the new engine yet.

Reply:

> The doc_preparer and doc_indexer threads and their queues in your profile were deleted; bulk indexing now runs as tasks on the shared worker pool, on a different search engine. A first local smoke run still had bulk indexing slower than Elasticsearch, so I won't claim parity. The numbers will come from a published benchmark against Elasticsearch and Quickwit on the new engine, with the losses included.

Cites: `docs/status/phase7c-x3.md:35` (`DocPreparer`, the `DocIndexer` threads and pools, and their options deleted; `grep DocPreparer src` is empty), `docs/perf/bulk-index-profile.md:17` (4,510 docs/s, the pre-strata profile), `docs/perf/closing-baseline-2026-11-04/README.md` table (T1: strata behind Elasticsearch by 3.47x on documents/s in a 200,000-document smoke run, "not a published or gate measurement"), PLAN.md Goals (the closing benchmark). The smoke figure is a draft-reply fact: owner decides whether to say it.

Detent: yes. The write path's hand-offs between preparing and indexing threads (a spin-locked queue) were the cost; the same shape appears in any group-commit or replicated-log append path, and the closing benchmark's write rows are the reference.

### #28 Google Cloud Run

Verdict: cannot verify here. The call in the report is a `setsockopt` of level 1 (SOL_SOCKET), option 13 (SO_LINGER), length 8 (a `struct linger`); that is an inference from the numbers. Nothing in `src/` sets it.

Reply:

> We can't reproduce this without Cloud Run. The call in the message is setsockopt with SOL_SOCKET and SO_LINGER (13, a struct linger), and the current source does not set SO_LINGER; the server now runs on Asio rather than libev, and discovery sockets are gone. Could you retry with the latest image from ghcr.io/kronuz/xapiand? Cloud Run also passes the port in $PORT, so start it with `--port` set to that, and mount a volume for the data directory. If there is no reply we'll close this.

Cites: no `SO_LINGER` or `linger` in `src/` (grep) or in the reactor and http repositories outside their tests and docs (`grep -rI linger ~/code/Kronuz/reactor ~/code/Kronuz/http`), `src/opts.cc:192` (`--port`), `src/opts.cc` (no discovery options), `Dockerfile:88-90` (EXPOSE 8880, entrypoint). Asio itself could issue the call (not checked; its sources were not read). Not run: any container on gVisor.

Owner: "not planned" if the reporter does not retest.

Detent: yes. Sandboxed runtimes (gVisor, some seccomp profiles) reject socket options the shared libraries might set; a check of `reactor`, `io` and `http` for options outside the common set protects every user of them.

### #29 compile with cmake : linking (error)

Verdict: fixed. The link line now carries libcap.

Reply:

> This was a missing link against libcap, fixed in 009cfac58 (January 2020, "Added missing library link"). CMake now finds libcap and puts it on the link line of xapiand, and the Dockerfile and release workflows install libcap-dev to build with it. If you still see it on a current checkout, please open a new issue with the compiler and CMake versions.

Cites: `CMakeLists.txt:375-381` (`find_library(CAP_LIBRARIES cap)`) and `:1245` (`${CAP_LIBRARIES}` on the link), `Dockerfile:44`, `.github/workflows/release.yml:145,216`, `git show 009cfac58`. Not run: a Linux build of the current tree on Ubuntu 18.04 / GCC 8 (the reporter's); the tree needs C++20.

Detent: no.

### #33 License question

Verdict: answered, and the wording is the owner's. Releases through alpha.9 link Xapian; master does not; two Xapian-derived files remain until the sortable-encoding work lands.

Reply:

> Releases up to v1.0.0-alpha.9 link Xapian, which is GPL, so treat those binaries as GPL. The development tree no longer uses Xapian: the engine is strata (MIT). Two files derived from Xapian, `src/sortable_serialise.{cc,h}` and the Python client's `sortable.py`, are still GPL and are being replaced; the first release without them will be fully MIT. An application that only talks to the server over HTTP does not link it, but I can't give legal advice; ask your counsel.

Cites: `docs/src/content/docs/changelog.md:244-246` (Xapian engine removed), `CMakeLists.txt` (no Xapian; grep), `docs/status/license-audit.md` "Summary" (blockers 1 and 2), `README.md:127-129`, `docs/src/content/docs/copyright.md` (the note on `sortable_serialise`), PLAN.md D44 (sortable-encoding) and the quest `license-closure`. The Xapian-era binaries' status is a legal reading: owner confirms before it is posted.

Detent: yes. Detent shares the MIT libraries; the license audit's notice work (a `THIRD-PARTY-NOTICES` file installed with each package) applies to its binaries too.

### #34 upgrade to Xapian 1.4.16

Verdict: answered as removed.

Reply:

> There is no plan to upgrade Xapian: it has been removed from the development tree, where the search engine is now strata, and Xapian databases are no longer read. A final release of the Xapian-based line will be cut from the `xapiand` branch with long-standing bug fixes backported; it keeps the Xapian it vendors. After that release, upgrading means dumping with it and restoring into the new version. Closing as not planned.

Cites: `docs/src/content/docs/changelog.md:244-246` ("Databases in Xapian's format are not read: dump them with the final Xapian-only release"), `CMakeLists.txt` (no Xapian), `.campaign/xapiand/quests/xapiand-final-release.md` Outcome, PLAN.md D34. Which Xapian version the `xapiand` branch vendors was not checked.

Owner: "not planned".

Detent: no.

### #35 DELETE all  documents

Verdict: new work, already a hunt row. `DELETE /index/` still answers 501; there is no delete-by-query.

Reply:

> `DELETE` on an index path still answers 501 Not Implemented, as in your report; the docs page for it says so. Deleting a whole index over HTTP is on the work list, since the engine can drop an index. Until then, delete documents by id; there is no delete-by-query. I'll update this issue when the endpoint exists.

Cites: `src/server/search_views.cc:1478-1491` (`delete_database_view` answers 501 with a path, 405 without), `docs/src/content/docs/reference-guide/indexes/delete-api.md` (the banner and the 501 E2E), `.campaign/xapiand/hunt.md:28` (the row), `docs/design/xapiand-on-strata.md:97` (delete by query: Xapiand never calls it). The reply offers no workaround beyond deleting by id (`DELETE /index/type/id`, `docs/src/content/docs/reference-guide/documents/delete-api.md`); not run.

Detent: yes. Dropping an index on every replica (tombstone, then reclaim) and keeping the drop from racing in-flight writes is the replicated store's index-drop path.

### #39 Is this project dead?

Verdict: works now. Active.

Reply:

> No. Development is active: the tree has had several hundred commits in the last month, and the v1.0.0-alpha.9 release was on 2026-10-17. The search engine was replaced with strata and Xapian removed, the Linux, macOS and FreeBSD builds and packages are in CI, and the docs are being rewritten. The next release is the first on the new engine; it has no date. Questions and bug reports here are welcome.

Cites: `git log master --since=2026-10-10 --oneline | wc -l` is 938 (it counts campaign bookkeeping commits, so "several hundred" is conservative on code), `gh release list` (alpha.9, 2026-10-17), `docs/src/content/docs/changelog.md:244`, `.github/workflows/ci.yml`, `.github/workflows/freebsd.yml`.

Detent: no.

## Proposed hunt rows

For the lead to file in `.campaign/xapiand/hunt.md`. Format: target, evidence, start in, repo.

| Target | Evidence | Start in | Repo |
|---|---|---|---|
| H1 (#14): n-gram term generation for text fields (`_ngram`), or delete the inert `_ngram` flag; Elasticsearch and Tantivy serve n-gram analysis | `_ngram` is read into `flags.ngram` (`src/database/schema.cc:7018`) and never reaches `GenerateTerms::text` (`src/multivalue/generate_terms.cc:521-540`), which takes only `cjk_ngram`; `analysis/README.md` lists CJK n-grams only | `src/multivalue/generate_terms.cc:521-540`, `src/database/schema.cc:5631,5663,5673`, analysis `term_generator` | analysis, Xapiand |
| H2 (#18): a per-search time limit that cancels the search at its next window boundary (an `opts` default and a URL parameter), answering 503 or a partial result with `timed_out` as the owner chooses | no timeout in `src/opts.cc` or `search_views.cc`; the cancellation exists (`src/engine/search.h:91-93`, `src/server/search_views.cc:2321,3132,3200-3203`) | `src/server/search_views.cc` search view, `src/engine/search.h` budget | Xapiand |
| H3 (#21): result highlighting (re-run a stored text field through its analyzers to mark the matched words); an owner scope call under D40 (peers serve it, it fits the engine) | no `highlight` or `snippet` in `src/`; the Xapian snippet call the issue names is gone | `src/server/search_views.cc` response build, analysis `term_generator` | Xapiand, analysis |
| H4 (#35): serve `DELETE /index/` (and decide on delete by query) | `src/server/search_views.cc:1478-1491` answers 501; already filed at `.campaign/xapiand/hunt.md:28`, so no new row unless delete by query is wanted | `src/server/search_views.cc:1478-1491`, `src/database` | Xapiand |
| H5 (#25 comment): an accent-folding option for text fields (`poeme` finds `poème`); Elasticsearch's `asciifolding` and Tantivy's `AsciiFoldingFilter` do it (peers from memory, unchecked) | the reporter's comment; `analysis/README.md:10` lists case folding only and `analysis.hh` has no accent filter | analysis `term_generator`, `src/multivalue/generate_terms.cc` | analysis |
| H6 (#25): document `_store` (index a field without keeping its value) with E2E blocks | `feed_store` exists (`src/database/schema.cc:7209`); no docs page mentions `_store` (grep); D47 requires a page and tests for every served feature | `docs/src/content/docs/reference-guide/schemas/` | Xapiand |

## Owner calls

One batch, as the quest asks.

1. Close as "not planned": #24 (glossary of cluster terms), #26 (cluster without multicast), #34 (upgrade Xapian). Each is a removed feature answered as removed.
2. #25: close with the reply, the at-rest encryption part as "not planned".
3. #28: close as "not planned" if the reporter does not retest after the reply.
4. Confirm the wording of #33 (license) and the Xapian-era GPL reading before it is posted, and whether #27's reply may state that a smoke run was behind Elasticsearch.
5. #21 (highlighting) and #14 (n-grams) are scope calls under D40 before they become quests.

## For Detent

One-line improvement notes the lead can forward. Only the issues with new work or a fix are listed here; the column above marks the rest "no" or answered.

1. Query deadlines (#18): give a fan-out search one deadline that every shard leg inherits and cancels on, built on the shared `task` cancellation token and budget (PLAN.md D20 names Detent's lead the owner of that design).
2. Encryption at rest (#25): encrypt data files and every replica's copy (a key per volume or per segment, kept off the node), and say what crosses the replication stream in clear.
3. Peer discovery without multicast (#26): bootstrap from a static seed list, DNS names, or a join through any live member, for deployments spread across networks.
4. Write path (#27): remove hand-offs through spin-locked queues in the append path; measure it with the closing benchmark's write rows.
5. Index drop (#35): drop an index on every replica (tombstone, then reclaim) without racing in-flight writes.
6. Packaging (#11): ship one set of packages per release (deb, rpm, image) with a service unit and a per-node data-directory layout.
7. Sandboxed runtimes (#28): check `reactor`, `io` and `http` for socket options gVisor and seccomp profiles reject, so the shared libraries run on Cloud Run-style hosts.
8. Glossary (#24): one docs page defining cluster, node, shard, replica and leader and how they relate.
9. Notices (#33): install `THIRD-PARTY-NOTICES` and `LICENSE` with each package, and keep GPL-derived code out of the shared libraries.
