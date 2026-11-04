> Status: CURRENT (2026-11-03) Phase 8 license audit of Xapiand master at 432299ed0, for the first MIT release on strata.

# License audit

## Summary

Three blockers stand between master and an MIT source and binary release; everything else is a notice or a header fix.

1. **`src/sortable_serialise.{cc,h}`** are GPL (Xapian origin, per ARCHITECTURE.md's licensing note) and are compiled into
   the binary: `src/*.c*` is globbed into `XAPIAND_SRC_LIST` and nothing removes them, and `src/serialise.h`,
   `src/database/utils.h`, `src/engine/strata/shard.cc` and `tests/test_engine_strata.cc` include the header. The clean-room
   replacement already planned for Phase 8 removes it.
2. **`contrib/python/xapiand-py/xapiand/utils/sortable.py`** is a Python port of the same routine under a GPL-2.0-or-later
   header (Copyright Olly Betts 2007-2016). It ships in the source tree, not in the binary packages.
3. **`docs/public/assets/Lenna.{jpg,png,pdf}`** is the Lenna test image, a 1972 Playboy photograph with no license.
   The docs' store-api page uses it as an upload example; any image of the owner's own does the job.

Every FetchContent dependency is compatible: MIT, BSD-2/3, BSL-1.0, zlib, Unicode, Unlicense and Apache-2.0 (one Lucene
port in prospect). None is copyleft. What the release lacks is notices: no package installs `LICENSE` or a third-party
notice file, the root `NOTICE` and the docs' copyright page still describe a GPL Xapian tree, and seven Kronuz libraries carry
third-party notices that need a binary notice only in source headers, not in their `LICENSE` files, so a notice file built
from those files misses them.

Headers: 20 files written in October 2026 say `Copyright (c) 2026 Dubalu LLC` and nothing else, which breaks the owner's rule
outright. 102 files added since 2021 and 21 older ones carry no header; most are harness scripts and the docs site, but 17
compiled `src/` files are among them, and a few headerless files are third-party code without their notice.

Counts: **3 blockers**, **27 actions** (A1-A27 below), the rest fine.

## Method

- Read `LICENSE`, `NOTICE`, `AUTHORS` and ARCHITECTURE.md's licensing note first. There is no `COPYING`.
- `src/sortable_serialise.{cc,h}` were never opened. One `git grep` for the files that include the header also
  matched the `.cc`'s own `#include "sortable_serialise.h"` line (line 23); no other content of either file was seen.
- Own tree: the first 30 lines of every tracked code file (C/C++, Python, shell, CMake, JS/TS, Astro, Ruby, Dockerfile;
  `docs/discussions/` separately), each file's copyright lines, its license text, and its creation date from
  `git log -M --name-status` (renames followed).
- Dependencies: the configured tree `.scratch/build-repin/_deps` (78 sources). That tree was configured from the working
  tree, which pins task, strata and prospect one commit past master; their `LICENSE` files at master's pins (task
  d0807866, strata 3f564dc5, prospect bd782248) were read with `git show` and are identical MIT texts, and `LICENSE`,
  `NOTICE` and `LICENSES/` have no diff between the two pins. Each dependency's license files were classified, and every
  copyright line in the Kronuz libraries that names someone other than the owner or Dubalu was traced to its file.
- npm: `npx license-checker --summary` and `--json` in `docs/` (313 installed packages).

## Own tree

### License files

| File | Finding | Mark |
|---|---|---|
| `LICENSE` | MIT; "Copyright © 2026 Germán Méndez Bravo (Kronuz)" and "Copyright © 2015-2020 Dubalu LLC." | fine |
| `NOTICE` | Stale: says Dubalu owns any file without a header, lists GPL-2 among the tree's licenses, has a Xapian section about GPL files, and gives Dubalu 2015-2019 (the LICENSE says 2015-2020). "All rights reserved" sits oddly beside MIT. Rewrite it as the third-party notice file (A1). | **A1** |
| `docs/src/content/docs/copyright.md` | The same stale text, published on the site; `about.md` links to it. Rewrite with A1. | **A2** |
| `README.md`, `ARCHITECTURE.md` (licensing note), `AGENTS.md` | Say `sortable_serialise` is GPL "until the value encoding is replaced". Correct today; update when blocker 1 closes. | **A3** |
| `AUTHORS` | Lists the owner at `german.mb@dubalu.com`; the identity rule normalizes it to `german.mb@gmail.com`. Not a license issue. | **A4** |

### Blockers in the tree

| Path | License | Why | Mark |
|---|---|---|---|
| `src/sortable_serialise.cc`, `src/sortable_serialise.h` | GPL (Xapian) | Compiled into the binary; the combined binary is bound by it. Clean-room replacement in Phase 8. | **blocker** |
| `contrib/python/xapiand-py/xapiand/utils/sortable.py` | GPL-2.0-or-later (Olly Betts) | Port of the same routine. Replace it with a port of the clean-room encoding, or drop it from the client. | **blocker** |
| `docs/public/assets/Lenna.jpg`, `.png`, `.pdf` | none (Playboy, 1972) | Redistributed without any license; used by `reference-guide/documents/store-api.md`. | **blocker** |

### Headers that break the owner's rule

| Files | Header | Mark |
|---|---|---|
| `src/engine/search.cc`, `src/engine/text.{cc,h}`, `src/engine/strata/{doc_view,expand,pattern,search}.{cc,h}`, `src/engine/strata/viewed.h`, `src/exception_engine.h`, `tests/fuzz_body.cc`, `tests/fuzz_strata_pattern.cc`, `tests/fuzz_text.cc`, `tests/test_engine_callbacks.cc`, `tests/test_engine_fields.cc`, `tests/test_engine_search_strata.cc`, `tests/test_text.cc` (20 files, all added 2026-10-27/28) | `Copyright (c) 2026 Dubalu LLC` only. New work: replace with "Copyright © 2026 Germán Méndez Bravo (Kronuz)". | **A5** |
| `src/tls.{cc,h}`, `src/debounce_or_run.h`, `src/package_hash.h.in`, `src/xapiand_strings.hh`, `src/xapiand_fsyncher.h`, `src/xapiand_storage_io.h`, `src/xapiand_io_random_errors.h`, `src/xapiand_cuuid_node.h`, and the trace seams `src/xapiand_{cuuid,fs,io,scheduler,stash,storage,system,threadpool}_trace.h` (17 files, added 2026), `tests/lua_patch_drift.cmake` | No header on compiled or test code written in 2026. Add the 2026 MIT header. | **A6** |
| `harness/` (55 scripts), docs site code (`docs/astro.config.mjs`, `docs/src/*.ts`, `docs/src/components/*.astro`, `docs/src/lib/*.mjs`, `docs/src/plugins/*.mjs`; 21 files), `docs/perf/t11-barriers/raw/` (4), `Dockerfile`, `contrib/homebrew/xapiand.rb`, `contrib/lua/lua-version.cmake`, `cmake/Modules/GenPackageHash.cmake` | No header; 2026 work. The root `LICENSE` covers them, so this is consistency, not compliance; add the header or record that tooling is exempt. | **A7** |
| `config.h.in`, `package_config.h.in`, `cmake/cmake_uninstall.cmake.in`, `cmake/Modules/RemoveDuplicateSubstrings.cmake`, `cmake/Modules/AC_HEADER_STDC.cmake`, `docs_to_postman.py`, `contrib/docker/xapiand/entrypoint.sh`, `contrib/python/multicast.py`, `contrib/python/cuuid/mertwis.py`, `docs/public/assets/{generator,update_formats}.py`, `contrib/python/xapiand-py/{setup.py,MANIFEST.in}` | No header; 2015-2020 Dubalu-era files. Add the two-line header (owner 2026 plus Dubalu 2015-2020) to the code files. | **A8** |
| `src/datetime_msgpack.{cc,h}`, `src/metric_serialise.h`, `src/server/search_service.{cc,h}`, `contrib/python/xapiand-py/xapiand/connection/http_aiohttp.py` | Added in 2026 but carry a Dubalu 2015-2019 line. Each was split or rewritten from Dubalu-era code (datetime extraction, metric de-vendor, server rename, client rewrite), so the Dubalu line follows its content. | fine |
| 105 + 42 + 2 files with owner 2026 plus Dubalu 2015-2018/2019/2020, 70 with owner 2026 only, `src/metrics.{cc,h}` (Dubalu 2018-2019), `src/node.h`, `src/database/shard.cc` | Match the rule. Ten `cmake/Modules` and `contrib` files spell it `Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.`; cosmetic. | fine |
| `src/length.{cc,h}` | MIT under Olly Betts' copyright (2006-2012) next to the owner's and Dubalu's, granted by him in commit `Xapiand:1691e5b2e` ("Got permission by olly to include length.h and length.cc with MIT license"). List him in the notice file. | fine (notice in A1) |
| `docs/discussions/` (44 files) | No license file or headers; its README says to "choose a license" before publishing it on its own. Inside Xapiand the root MIT `LICENSE` covers it. Byte-identical with `~/code/KronuzBlog/Kronuz.github.io/discussions`; `~/code/KronuzBlog/gmendezb-pages/discussions` differs in `worker/package-lock.json` (sync drift, not a license issue). Adding a `LICENSE` there has to land in all three copies. | **A9** |

### Third-party code and data in the tree

| Path | Origin and license | Mark |
|---|---|---|
| `contrib/python/xapiand-py/` (whole client) | A fork of elasticsearch-py: Apache-2.0 `LICENSE`, headers "Copyright (c) 2017 Elasticsearch" (19 files), `setup.py` declares Apache-2.0. Compatible as a separately licensed subcomponent; name it in the notice file and the docs' copyright page. Not in any binary package. | **A10** |
| `contrib/javascript/cuuid/src/bx.js` | Fork of cryptocoinjs/bs58 (MIT; Copyright 2011 Google Inc, 2013 BitPay Inc). Copyright lines kept, MIT permission text missing. Add it. | **A11** |
| `contrib/javascript/cuuid/src/utilsUUID.js` | node-uuid, MIT, Robert Kieffer; full text present. | fine |
| `contrib/geospatial/google_map_plotter.py` | gmplot (Michael Woods), "License: MIT" without the copyright line or text. Add them. | **A12** |
| `src/y2j/y2j.{cc,h}` | No header. Probably tangrams/y2j (MIT, Mapzen), from the API shape (`y2j::yamlParseBytes`, libyaml to RapidJSON); unconfirmed. Confirm and restore its notice, and list it in A1. | **A13** |
| `cmake/Modules/AddCXXCompilerFlag.cmake`, `cmake/Modules/GetGitVersion.cmake` | Text matches google/benchmark's modules (Apache-2.0), notice missing. Build-only, not in the binary. Add the Apache header or rewrite. | **A14** |
| `cmake/Modules/CheckPrototypeExists.cmake` | Says it was downloaded from KDE's kdelibs (BSD-3 there); notice missing. Build-only. | **A15** |
| `cmake/Modules/CheckDIRSymbolExists.cmake` | Unattributed; its wording matches PLplot's module, whose license is unconfirmed (PLplot is LGPL). Used only by `AC_HEADER_STDC.cmake`. Confirm, or delete the `AC_HEADER_STDC` chain in favor of CMake's `CheckSymbolExists`. | **A16** |
| `cmake/Modules/FindGTest.cmake` | Owner's module; fetches googletest (BSD-3) at `master`, tests only. Unpinned, which is a reproducibility issue, not a license one. | fine |
| `src/multivalue/generate_terms.cc:102` | Cites a Stack Overflow answer for a design note (why `unique_ptr`); no code copied. | fine |
| `src/xapiand_namegen.h` | Thin seam over namegen (fantasyname, public domain). | fine |
| `stopwords/*.txt` (18 lists, installed by `make install` into every package) | Added in 2016 with no source recorded; `en.txt` (569 words, "a a's able about") is the SMART stop list. Record each list's source and license in the notice file, or regenerate them from Snowball's BSD-3 lists, which the snowball dependency already carries. | **A17** |
| `mime.types` (installed into every package) | nginx-style `types {}` table with h5bp-like grouping comments; no source recorded. Record it (nginx BSD-2, h5bp MIT) or regenerate. | **A18** |
| `docs/public/assets/el_cerrito.png` | Google Maps screenshot with "Map data ©2019 Google" attribution, overlaid with HTM trixels. Google's geo guidelines allow this in documentation with the attribution kept; redraw on OpenStreetMap tiles to remove the dependency on that permission. | **A19** |
| `docs/public/assets/htm.png`, `htm.svg`, `assets/runtime.svg` | No source recorded for `htm.png` (HTM subdivision renders); record it. The SVGs are the owner's. | **A20** |
| `docs/public/assets/generator.py` and the `accounts.*` fixtures it produced | Faker plus phrase tables copied from fantasynamegenerators.com (cited in the docstring), which grants no license. Short phrases, low risk; rewrite the tables in the owner's words. | **A21** |
| `docs/public/assets/cities.*`, `fruits.ndjson`, `accounts-generator.json` | City names, coordinates and populations (facts); a json-generator template. | fine |
| `harness/` corpora | Inline only: `index_fortune`'s ten proverbs (falls back to the system `fortune(6)` without copying it), `analysis_diff.py`'s own test sentences, the accounts fixture. No downloaded corpus is tracked. | fine |
| `docs/src/content/docs/` | `NOTICE` says the docs are "heavily based upon" Elasticsearch's (Apache-2.0 at the time). Keep that attribution in the rewritten notice and copyright page. | **A22** |

## FetchContent dependencies

All at their pinned commits in `CMakeLists.txt`. "Binary notice" means the license requires its text in a binary distribution.

### Third-party upstreams

| Dependency | Pin | License | Binary notice | Mark |
|---|---|---|---|---|
| asio (chriskohlhoff) | `asio-1-36-0` (RPM job: 1.28.1 tarball) | BSL-1.0 | no | fine |
| rapidjson (Tencent) | 24b5e7a8 | MIT (its `msinttypes` BSD-3 is MSVC-only; `bin/jsonchecker` under the JSON license is test data, not compiled) | yes | fine, notice in A1 |
| cppcodec (tplgy) | `v0.2` | MIT | yes | fine, notice in A1 |
| CLI11 | `v2.4.2` | BSD-3 | yes | fine, notice in A1 |
| lz4 | `v1.8.1` | `lib/` BSD-2 (with xxhash), the rest GPL-2; only `lib/*.c` is compiled | yes | fine, notice in A1 |
| prometheus-cpp | `v1.2.4` | MIT | yes | fine, notice in A1 |
| Lua | 5.4.7 tarball (sha256 pinned) plus `contrib/lua/xapiand-limits.patch` | MIT (in `lua.h` and `doc/readme.html`; the tarball has no `LICENSE` file) | yes | fine, notice in A1 |
| libyaml | `0.2.5` | MIT | yes | fine, notice in A1 |
| googletest | `master` (`FindGTest.cmake`), tests only | BSD-3 | not shipped | fine |
| cpp-btree (Kronuz fork of Google's) | strata bench target only | Apache-2.0 | not shipped | fine |

### Kronuz libraries

The other 70 are MIT (namegen is Unlicense) under the owner (2026) and, for the libraries extracted from Xapiand, Dubalu LLC (2015-2020): allocators,
analysis, atomic-shared-ptr, base-x, bitpack, boolean-parser, cartesian, char-classify, completion, compressors,
constexpr-phf, crc32c, cuuid, datetime, docset, double-metaphone, endian, enum-reflection, epoch, errno-names, fs, fst,
fusefilter, hashes, htm, http, http-log, http-parser, io, iterators, located-exception, logger, lru-cache, math, md5,
metrics, msgpack, namegen, nanosleep, prospect, queryparser, radix-router, random, reactor, repr, residency,
scheduler, sha256, snowball, soundex, split, stash, static-string, storage, strata, strict-stox, string-similarity,
stringified, strings, system, task, term-color, termdict, threadpool, traceback, uinteger-t, unicode, url-parser, utype,
varint. Their own notice is the same as Xapiand's `LICENSE`. These embed third-party code:

| Library | Embedded code | License | Notice in the library's `LICENSE`? | Mark |
|---|---|---|---|---|
| prospect | `required_optional_scorer` pruning ported from Lucene 10.4.0 `ReqOptSumScorer` | Apache-2.0; `NOTICE` and `LICENSES/Apache-2.0.txt` present | `NOTICE` yes | fine; carry its NOTICE into Xapiand's (A1) |
| snowball | Snowball stemmers and runtime | BSD-3 (`LICENSE-SNOWBALL`) | yes | fine, notice in A1 |
| unicode | Unicode Character Database tables | Unicode License (`LICENSE-UNICODE`) | yes | fine, notice in A1 |
| http-parser | nodejs/http-parser (Joyent, nginx's Igor Sysoev) and llhttp (Fedor Indutny) | MIT | yes | fine, notice in A1 |
| uinteger-t | Jason Lee's uint128_t | MIT | yes | fine, notice in A1 |
| static-string | Andrzej Krzemienski's static string | BSL-1.0 | yes | fine |
| msgpack | msgpack-c (FURUHASHI Sadayuki, KONDO Takatoshi) and Boost.Preprocessor | BSL-1.0 (`LICENSE_1_0.txt`) | headers only | fine (BSL needs no binary notice) |
| allocators | memory pool from moya-lang.org (2017) | BSD-2 | headers only | **A23** |
| compressors | `xxh32_stream.h`, xxHash (Yann Collet) | BSD-2 | headers only | **A23** |
| enum-reflection | Better Enums (Anton Bachin) | BSD-2 | headers only | **A23** |
| boolean-parser | furan (2014) | MIT | headers only | **A23** |
| cuuid | crossguid (Graeme Hill) | MIT | headers only | **A23** |
| hashes | Daniel Kirchner (2015) | MIT | headers only | **A23** |
| scheduler | `callable_traits.hh`, Stephan Hohe | MIT | headers only | **A23** |
| md5, sha256 | Stephan Brumme | zlib | headers only | fine (zlib needs no binary notice) |

A23 is fixed in each library's repository (add the third-party notices to its `LICENSE` or a `NOTICE`), so Xapiand's notice
file can be generated from the fetched `LICENSE*`/`NOTICE` files instead of from source headers.

### System libraries (dynamically linked; package dependencies, not bundled)

OpenSSL 3 (Apache-2.0), zlib (zlib), libuuid (BSD-3), zstd (BSD-3), libcap (BSD-3), execinfo/rt/dl/m (system). fpm
declares them as `--depends`, the Docker runtime stage installs them from Ubuntu with their own `/usr/share/doc/*/copyright`,
and Homebrew links its own formulae. **fine**

## Docs site npm dependencies

`npx license-checker --summary` in `docs/`: MIT 326, ISC 9, BSD-2-Clause 9, Apache-2.0 5, BSD-3-Clause 5,
BlueOak-1.0.0 3, MPL-2.0 2, CC0-1.0 2, UNKNOWN 1, LGPL-3.0-or-later 1, Python-2.0 1, MIT* 1, UNLICENSED 1.

| Package | License | Note | Mark |
|---|---|---|---|
| `@img/sharp-libvips-darwin-x64` | LGPL-3.0-or-later | sharp's libvips, build-time image processing; nothing ships | fine |
| `lightningcss`, `lightningcss-darwin-x64` | MPL-2.0 | build-time CSS; nothing ships | fine |
| `lru-cache`, `sax`, `common-ancestor-path` | BlueOak-1.0.0 | permissive | fine |
| `argparse` | Python-2.0 | permissive | fine |
| `@bruits/satteri-darwin-x64`, `satteri` | UNKNOWN, MIT* | platform binary of satteri, whose package is MIT | fine |
| `xapiand-docs` | UNLICENSED | the docs package itself (`"private": true`, no `license` field); add `"license": "MIT"` | **A24** |

The built site ships Astro and Starlight runtime code (MIT). The Discussions worker's dependencies (hono, the
remark/rehype family, shiki; wrangler and typescript for development) are MIT or Apache-2.0. **fine**

## Binary packaging

`ninja install` stages only the binary, `stopwords/`, `mime.types` and `man/xapiand.8`; no license or notice file reaches any
artifact.

| Artifact | Today | Needs | Mark |
|---|---|---|---|
| All (deb, rpm, FreeBSD pkg, Docker, Homebrew bottle) | No `LICENSE`, no notices | `install(FILES LICENSE NOTICE ...)` into `share/doc/xapiand` (Debian policy wants `/usr/share/doc/xapiand/copyright`), so every artifact built from the install tree carries them | **A25** |
| `NOTICE` (rewritten as third-party notices) | Stale | Xapiand's MIT notice; the full texts or required notices for rapidjson, cppcodec, CLI11, lz4 lib and xxHash, prometheus-cpp, Lua, libyaml, Snowball, the Unicode data, http-parser and llhttp, uint128_t, the A23 embeds, Olly Betts' `length` permission; Lucene's NOTICE lines from prospect; the BSL-1.0 and zlib components listed by name; the stopword and mime.types sources (A17, A18). Generate it from `_deps/*-src/{LICENSE*,NOTICE}` at the release pins so it tracks repins. | A1 |
| deb, rpm (`release.yml`), FreeBSD pkg (`freebsd.yml`) | `--license MIT`, `licenses: [MIT]` | MIT is accurate for Xapiand's own code. After blocker 1, use an SPDX expression for the bundled code (`MIT AND BSD-2-Clause AND BSD-3-Clause AND BSL-1.0 AND Zlib AND Apache-2.0 AND Unicode-3.0`), or keep MIT and rely on the notice file | **A26** |
| Docker image | `org.opencontainers.image.licenses="MIT"`; Ubuntu runtime packages keep their own copyright files | Same label decision as A26; A25 covers the notice file | A26 |
| Homebrew formula | `license "MIT"`, head-only, builds from source | Same as A26 (Homebrew accepts `all_of: [...]`); bottles get the notice file through A25 | A26 |
| Source release (GitHub tag tarball) | Whole tree, including `contrib/` | Blockers 2 and 3 apply to it as well as blocker 1; the Apache-2.0 client (A10) is fine once named | blockers |
| Sortable encoding after replacement | — | Re-run this audit's dependency and header scans at the release commit; the scripts are short and can live under `harness/` | **A27** |

## Actions

1. **A1**: Rewrite `NOTICE` as the third-party notice file.
2. **A2**: Rewrite `docs/src/content/docs/copyright.md` to match.
3. **A3**: Update README, ARCHITECTURE and AGENTS licensing text when `sortable_serialise` is gone.
4. **A4**: Normalize the owner's address in `AUTHORS`.
5. **A5**: Fix the 20 `Copyright (c) 2026 Dubalu LLC` headers.
6. **A6**: Add headers to 17 compiled `src/` files and one test file.
7. **A7**: Add headers to harness, docs site and build files, or exempt tooling explicitly.
8. **A8**: Add headers to the Dubalu-era headerless files.
9. **A9**: Give `docs/discussions` a license in all three copies.
10. **A10**: Name the Apache-2.0 Python client in the notices.
11. **A11**: Add bs58's MIT text to `bx.js`.
12. **A12**: Add gmplot's copyright and MIT text.
13. **A13**: Confirm y2j's origin and restore its notice.
14. **A14**: google/benchmark CMake modules: add the Apache header or rewrite.
15. **A15**: KDE `CheckPrototypeExists.cmake`: add the BSD notice.
16. **A16**: Confirm `CheckDIRSymbolExists.cmake`'s license or delete the chain.
17. **A17**: Record or regenerate the stopword lists.
18. **A18**: Record or regenerate `mime.types`.
19. **A19**: Keep or redraw the Google Maps screenshot.
20. **A20**: Record `htm.png`'s source.
21. **A21**: Rewrite `generator.py`'s phrase tables.
22. **A22**: Keep the Elasticsearch docs attribution.
23. **A23**: Move embedded third-party notices into seven Kronuz libraries' `LICENSE` files.
24. **A24**: Set `"license": "MIT"` in `docs/package.json`.
25. **A25**: Install `LICENSE` and `NOTICE` with the binary.
26. **A26**: Decide the package license metadata.
27. **A27**: Re-run the audit at the release commit.

## Ledger: license actions (branch `phase8/license-actions`)

State (2026-11-03): A1, A2, A4, A5, A6, A7, A8, A10, A11, A12, A13, A14, A15, A16, A20, A22, A23, A24 and A25 are done on the branch; the owner's actions (A3, A9, A17, A18, A19, A21, A26, A27) and the three blockers are untouched. Commit hashes carry the repository: `Xapiand:`.

| Action | Status | Evidence |
|---|---|---|
| A1 | done | `NOTICE:1-26` (owner 2026, Dubalu 2015-2020, MIT pointer, Elasticsearch section); `THIRD-PARTY-NOTICES` generated by `cmake/generate_third_party_notices.py` from the pinned `_deps`; rerun against a fresh configure at master's pins reproduces it byte for byte. Mechanism note `docs/mechanisms/third-party-notices.md`. The stopword and mime.types sources are not in it (A17, A18 are the owner's). |
| A2 | done | `docs/src/content/docs/copyright.md:6-31` matches `NOTICE`. |
| A4 | done | `AUTHORS:1` is `german.mb@gmail.com`. |
| A5 | done | `git grep -E 'Copyright \(c\) 2026 Dubalu'` matches only this document; `src/engine/search.cc:2`, `src/exception_engine.h:2`, `tests/fuzz_body.cc:2` carry the owner's line. |
| A6 | done | 17 `src/` files (`src/tls.cc:2`, `src/debounce_or_run.h:2`, `src/xapiand_*_trace.h:2`, ...) and `tests/lua_patch_drift.cmake:1` carry the 2026 MIT header. |
| A7 | done, with exemptions | Headers on 57 `harness/` scripts (`harness/env.sh:1`), the Dockerfile, `contrib/homebrew/xapiand.rb`, `contrib/lua/lua-version.cmake`, `cmake/Modules/GenPackageHash.cmake`, `docs/astro.config.mjs`, `docs/src/{consts,content.config,starlightRouteData}.ts`, `docs/src/lib/{normalizeAnsi,renderD2}.mjs`, `docs/src/plugins/*.mjs` and 9 `docs/src/components/*.astro` (inside the frontmatter, so nothing renders). Seven of the components are Starlight overrides (`Footer`, `PageTitle`, `Pagination`, `Sidebar`, `SidebarSublist`, `SiteTitle`, `SocialIcons`) and also name Starlight's MIT copyright (2023 Astro contributors). Exempt: `harness/{README.md,ruff.toml,pyrefly.toml,async_allowlist.tsv,.gitignore}` (data and config), `docs/src/lib/sidebar.mjs` (generated), `docs/perf/t11-barriers/raw/` (evidence kept byte for byte). |
| A8 | done | `config.h.in:2-3`, `package_config.h.in`, `cmake/cmake_uninstall.cmake.in`, `cmake/Modules/{RemoveDuplicateSubstrings,AC_HEADER_STDC}.cmake`, `docs_to_postman.py`, `contrib/docker/xapiand/entrypoint.sh`, `contrib/python/{multicast.py,cuuid/mertwis.py}`, `docs/public/assets/{generator,update_formats}.py`, `contrib/python/xapiand-py/setup.py` carry owner 2026 plus Dubalu 2015-2020. `RemoveDuplicateSubstrings.cmake`'s copied AddCXXCompilerFlag comment is rewritten. `contrib/python/xapiand-py/MANIFEST.in` is a data file, exempt. `setup.py` declares Apache-2.0 for the client as a whole; its new header names only the MIT parts' owners, which the client's other files already do. |
| A10 | done | `NOTICE:20-22`, `docs/src/content/docs/copyright.md:25-28`, and the generated `THIRD-PARTY-NOTICES` name `contrib/python/xapiand-py`, its Apache-2.0 `LICENSE` and Copyright (c) 2017 Elasticsearch, and that no package carries it. |
| A11 | done | `contrib/javascript/cuuid/src/bx.js:11-34` has bs58's MIT text with Google Inc 2011 and BitPay 2013. |
| A12 | done | `contrib/geospatial/google_map_plotter.py:14` has the MIT text and "Copyright (c) 2014-2019 Michael Woods", checked against gmplot's own LICENSE (`github.com/gmplot/gmplot`, which adds a 2020 line for later contributors that this older copy does not carry). |
| A13 | done, confirmed | The 2019 import commit `Xapiand:7fe1e367f` names `github.com/tangrams/y2j` at 45396f91. The upstream repository (checked 2026-11-03) is MIT, has `y2j.h` declaring `yamlParseBytes`, and an MIT `LICENSE` "Copyright (c) 2016 Mapzen"; `src/y2j/LICENSE:3` says 2016,2019 and stays as imported. `src/y2j/y2j.h:1-8` and `src/y2j/y2j.cc:1-8` now name the origin. The generated notice includes `src/y2j/LICENSE`. |
| A14 | done | `cmake/Modules/AddCXXCompilerFlag.cmake:1-5`, `cmake/Modules/GetGitVersion.cmake:1-5` name google/benchmark and Apache-2.0 (upstream's LICENSE has no copyright line, so none is given). |
| A15 | done | `cmake/Modules/CheckPrototypeExists.cmake:1-23` has KDE's BSD-3 text; the copyright line "Copyright (c) 2006, Alexander Neundorf" is the one kdelibs' file carries. |
| A16 | done by deletion | `cmake/Modules/CheckDIRSymbolExists.cmake` is gone; `cmake/Modules/AC_HEADER_STDC.cmake:56` runs `check_c_source_compiles` with the same test program; configure reports `HAVE_DIRENT_H - Success`. |
| A20 | done | `docs/public/assets/htm.png` (1800x1152) is a raster of `htm.svg` (the owner's Illustrator export): both arrive together in `Xapiand:d7b41caa4` (2019-02-20, the HTM docs commit), the aspect ratios match (1.5625 and 1.5617), and rendering the SVG with `rsvg-convert` shows the same five tessellated spheres as the PNG. Source: the owner's own artwork. |
| A22 | done | `NOTICE:24-26` and `copyright.md:30-31` keep the Elasticsearch documentation attribution. |
| A23 | done | Each repository's `LICENSE` ends with "Third-party notices": `allocators:0029140`, `compressors:5566260`, `enum-reflection:a360207`, `boolean-parser:f212b8c`, `cuuid:1df1c06`, `hashes:5791db9`, `scheduler:f080858`, pushed to each `main`. Latest CI at the time of the push: green on compressors, enum-reflection, boolean-parser, cuuid, hashes and scheduler before it; allocators' last scheduled nightly (2026-11-03 15:21Z, sha 07de45d) failed although the run on the same sha the day before passed, so that failure predates the commit. The pins in `CMakeLists.txt` are not moved. |
| A24 | done | `docs/package.json:6` and the root entry of `docs/package-lock.json` say `"license": "MIT"`. |
| A25 | done | `CMakeLists.txt:1309-1310` installs `LICENSE`, `NOTICE` and `THIRD-PARTY-NOTICES` into `share/doc/xapiand`. |

Build and test: a Release build (Ninja, `BUILD_TESTS=ON`, `LTO=OFF`, ccache) of the branch at `Xapiand:6a9218ef2` finished with exit 0, `ctest -j4` passed 37 of 37, and `cmake --install` staged `share/doc/xapiand/{LICENSE,NOTICE,THIRD-PARTY-NOTICES}`. The changes are comments, notice files and one CMake check (A16), so no mutant or revert test applies; a configure with the A16 replacement reported `HAVE_DIRENT_H - Success`. No fuzz target applies (no parser or decoder changed).

Not done, by instruction: A3 (waits for `sortable_serialise`'s removal), A9 (`docs/discussions` must stay byte-identical with two other copies), A17, A18, A19, A21, A26, A27. `docs/public/assets/Lenna.*` (blocker 3) is untouched on this branch; an earlier local branch `fix/license-actions` (never pushed, worktree `Xapiand-wt/license-actions`) also replaces those images with a generated sample, which this branch left out.

Design vs code: the audit asks for A1 as a rewritten `NOTICE` that carries the third-party texts; the code splits it into `NOTICE` (ownership, short) and a generated `THIRD-PARTY-NOTICES` (texts), both installed. No other differences.
