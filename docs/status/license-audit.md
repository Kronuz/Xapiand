> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-11-03) Phase 8 license audit of Xapiand master at 432299ed0, for the first MIT release on strata.

# License audit

## Summary

Three blockers stand between master and an MIT source and binary release; everything else is a notice or a header fix.

1. **`src/sortable_serialise.{cc,h}`** are GPL (Xapian origin, per ARCHITECTURE.md's licensing note) and are compiled into
   the binary: `src/*.c*` is globbed into `XAPIAND_SRC_LIST` and nothing removes them, and `src/serialize.h`,
   `src/database/utils.h`, `src/engine/strata/shard.cc` and `tests/test_engine_strata.cc` include the header. The clean-room
   replacement already planned for Phase 8 removes it.
2. **`contrib/python/xapiand-py/xapiand/utils/sortable.py`** is a Python port of the same routine under a GPL-2.0-or-later
   header (Copyright Olly Betts 2007-2016). It ships in the source tree, not in the binary packages.
3. **`site/public/assets/Lenna.{jpg,png,pdf}`** is the Lenna test image, a 1972 Playboy photograph with no license.
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
| `site/src/content/docs/copyright.md` | The same stale text, published on the site; `about.md` links to it. Rewrite with A1. | **A2** |
| `README.md`, `ARCHITECTURE.md` (licensing note), `AGENTS.md` | Say `sortable_serialise` is GPL "until the value encoding is replaced". Correct today; update when blocker 1 closes. | **A3** |
| `AUTHORS` | Lists the owner at `german.mb@dubalu.com`; the identity rule normalizes it to `german.mb@gmail.com`. Not a license issue. | **A4** |

### Blockers in the tree

| Path | License | Why | Mark |
|---|---|---|---|
| `src/sortable_serialise.cc`, `src/sortable_serialise.h` | GPL (Xapian) | Compiled into the binary; the combined binary is bound by it. Clean-room replacement in Phase 8. | **blocker** |
| `contrib/python/xapiand-py/xapiand/utils/sortable.py` | GPL-2.0-or-later (Olly Betts) | Port of the same routine. Replace it with a port of the clean-room encoding, or drop it from the client. | **blocker** |
| `site/public/assets/Lenna.jpg`, `.png`, `.pdf` | none (Playboy, 1972) | Redistributed without any license; used by `reference-guide/documents/store-api.md`. | **blocker** |

### Headers that break the owner's rule

| Files | Header | Mark |
|---|---|---|
| `src/engine/search.cc`, `src/engine/text.{cc,h}`, `src/engine/strata/{doc_view,expand,pattern,search}.{cc,h}`, `src/engine/strata/viewed.h`, `src/exception_engine.h`, `tests/fuzz_body.cc`, `tests/fuzz_strata_pattern.cc`, `tests/fuzz_text.cc`, `tests/test_engine_callbacks.cc`, `tests/test_engine_fields.cc`, `tests/test_engine_search_strata.cc`, `tests/test_text.cc` (20 files, all added 2026-10-27/28) | `Copyright (c) 2026 Dubalu LLC` only. New work: replace with "Copyright © 2026 Germán Méndez Bravo (Kronuz)". | **A5** |
| `src/tls.{cc,h}`, `src/debounce_or_run.h`, `src/package_hash.h.in`, `src/xapiand_strings.hh`, `src/xapiand_fsyncher.h`, `src/xapiand_storage_io.h`, `src/xapiand_io_random_errors.h`, `src/xapiand_cuuid_node.h`, and the trace seams `src/xapiand_{cuuid,fs,io,scheduler,stash,storage,system,threadpool}_trace.h` (17 files, added 2026), `tests/lua_patch_drift.cmake` | No header on compiled or test code written in 2026. Add the 2026 MIT header. | **A6** |
| `harness/` (55 scripts), docs site code (`site/astro.config.mjs`, `site/src/*.ts`, `site/src/components/*.astro`, `site/src/lib/*.mjs`, `site/src/plugins/*.mjs`; 21 files), `docs/perf/t11-barriers/raw/` (4), `Dockerfile`, `contrib/homebrew/xapiand.rb`, `contrib/lua/lua-version.cmake`, `cmake/Modules/GenPackageHash.cmake` | No header; 2026 work. The root `LICENSE` covers them, so this is consistency, not compliance; add the header or record that tooling is exempt. | **A7** |
| `config.h.in`, `package_config.h.in`, `cmake/cmake_uninstall.cmake.in`, `cmake/Modules/RemoveDuplicateSubstrings.cmake`, `cmake/Modules/AC_HEADER_STDC.cmake`, `docs_to_postman.py`, `contrib/docker/xapiand/entrypoint.sh`, `contrib/python/multicast.py`, `contrib/python/cuuid/mertwis.py`, `site/public/assets/{generator,update_formats}.py`, `contrib/python/xapiand-py/{setup.py,MANIFEST.in}` | No header; 2015-2020 Dubalu-era files. Add the two-line header (owner 2026 plus Dubalu 2015-2020) to the code files. | **A8** |
| `src/datetime_msgpack.{cc,h}`, `src/metric_serialize.h`, `src/server/search_service.{cc,h}`, `contrib/python/xapiand-py/xapiand/connection/http_aiohttp.py` | Added in 2026 but carry a Dubalu 2015-2019 line. Each was split or rewritten from Dubalu-era code (datetime extraction, metric de-vendor, server rename, client rewrite), so the Dubalu line follows its content. | fine |
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
| `site/public/assets/el_cerrito.png` | Google Maps screenshot with "Map data ©2019 Google" attribution, overlaid with HTM trixels. Google's geo guidelines allow this in documentation with the attribution kept; redraw on OpenStreetMap tiles to remove the dependency on that permission. | **A19** |
| `site/public/assets/htm.png`, `htm.svg`, `assets/runtime.svg` | No source recorded for `htm.png` (HTM subdivision renders); record it. The SVGs are the owner's. | **A20** |
| `site/public/assets/generator.py` and the `accounts.*` fixtures it produced | Faker plus phrase tables copied from fantasynamegenerators.com (cited in the docstring), which grants no license. Short phrases, low risk; rewrite the tables in the owner's words. | **A21** |
| `site/public/assets/cities.*`, `fruits.ndjson`, `accounts-generator.json` | City names, coordinates and populations (facts); a json-generator template. | fine |
| `harness/` corpora | Inline only: `index_fortune`'s ten proverbs (falls back to the system `fortune(6)` without copying it), `analysis_diff.py`'s own test sentences, the accounts fixture. No downloaded corpus is tracked. | fine |
| `site/src/content/docs/` | `NOTICE` says the docs are "heavily based upon" Elasticsearch's (Apache-2.0 at the time). Keep that attribution in the rewritten notice and copyright page. | **A22** |

## FetchContent dependencies

All at their pinned commits in `CMakeLists.txt`. "Binary notice" means the license requires its text in a binary distribution.

### Third-party upstreams

| Dependency | Pin | License | Binary notice | Mark |
|---|---|---|---|---|
| asio (chriskohlhoff) | `asio-1-36-0` (RPM job: 1.28.1 tarball) | BSL-1.0 | no | fine |
| rapidjson (Tencent) | 24b5e7a8 | MIT (its `msinttypes` BSD-3 is MSVC-only; `bin/jsonchecker` under the JSON license is test data, not compiled). Unmodified upstream: the build patches no fetched source (the one `PATCH_COMMAND` in `CMakeLists.txt` is Lua's), and only sets `RAPIDJSON_PARSE_DEFAULT_FLAGS` (comments, trailing commas, NaN/Inf, full precision) as a compile definition, which is configuration, not a derivative. The sanitizer patches that earlier gate notes mention (`stack.h`, `document.h`, `GenericStringRef`) applied to the old v1.1.0 pin in scratch trees only; `Xapiand:52235a037` moved the pin and nothing ships them | yes | fine, notice in A1 |
| cppcodec (tplgy) | `v0.2` | MIT | yes | fine, notice in A1 |
| CLI11 | `v2.4.2` | BSD-3 | yes | fine, notice in A1 |
| lz4 | `v1.8.1` | `lib/` BSD-2 (with xxhash), the rest GPL-2; only `lib/*.c` is compiled | yes | fine, notice in A1 |
| prometheus-cpp | `v1.2.4` | MIT | yes | fine, notice in A1 |
| Lua | 5.4.7 tarball (sha256 pinned) plus `contrib/lua/xapiand-limits.patch` | MIT (in `lua.h` and `doc/readme.html`; the tarball has no `LICENSE` file). The patch is the owner's and adds a few `lua_xnote` accounting calls to Lua's sources (a derivative under Lua's MIT, shipped only as the compiled result) | yes | fine, notice in A1 |
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

## Provenance of A17 and A18 (stopwords, mime.types)

Exploration only; fetched copies live in the ignored `.scratch/`. The Xapian copy was compared for provenance and deleted; none of its words appear here.

**A17, stopwords.** All 18 lists arrived in one commit, `Xapiand:b2858ab15` (2016-12-06, José María Valencia, not the owner): "Add stopwords for armenian(hy.txt), basque(eu.txt), catalan(ca.txt), ... languages". `git log --follow` and `git blame` show that commit for every line of every file; nothing was added later. The language set (ca da de en es eu fi fr hu hy it nl no pt ro ru sv tr) matches Lucene's per-language analyzers.

| English list | Size | Overlap with ours | Only in theirs | Only in ours |
|---|---|---|---|---|
| ours (`en.txt`) | 570 | | | |
| SMART (igorbrigadir/stopwords mirror) | 570 | 570 | 0 | 0 |
| Snowball English (BSD-3) | 174 | 164 | 10 | 406 |
| Lucene `EnglishAnalyzer` default (Apache-2.0; `StopAnalyzer` uses the same list) | 33 | 33 | 0 | 537 |
| NLTK English (Apache-2.0) | 198 | 165 | 33 | 405 |
| Xapian `xapian-core/languages/stopwords/english.txt` | 174 | 164 | 10 | 406 |

Verdict for `en.txt`: derived from SMART, word for word (set equality, 570 of 570), with no owner additions. Every word not in Snowball or NLTK is a SMART word (examples: `able`, `accordingly`, `ain't`, `allow`, `anybody`, `appreciate`, `apart`, `anyways`, `appropriate`, `becomes`), so there are no words only in ours beyond SMART. Xapian's list matches Snowball's counts and shares nothing extra with ours.

Other languages against Snowball's list (ours size, overlap, ours-only): da 101/50/51, nl 275/90/185, fi 772/49/723, fr 605/101/504, de 595/208/387, hu 781/197/584, it 623/279/344, no 172/172/0, pt 408/119/289, ru 539/158/381, es 577/166/411, sv 401/114/287. They are supersets of, or loosely related to, Snowball's lists (no, sv, it, hu, ru nearly contain them). Verdict: imported wholesale from third-party lists (Snowball and Lucene-family sources), not owner-curated; the exact per-language upstream is not recoverable from the history. The credit owed is the SMART list (Cornell, G. Salton and C. Buckley; its terms are not a standard license) and Snowball's BSD-3 notice. Safest fix is the audit's existing one: regenerate from Snowball's lists, which the build already carries.

**A18, `mime.types`.** Added in `Xapiand:a3abe894e` (2019-03-25, the owner, "Load mime types from file", 109 lines, the table lifted from the old `mime_types.hh`); the only later commit, `Xapiand:b88d75fc5` (2019-05-06, owner), added `yaml`/`yml`, `ndjson`, `msgpack`. Blame at the import: 109 of 109 lines the owner's commit, so no wholesale import is visible, but the content is nginx-format with h5bp-style section comments. Measured on (type, extension) pairs (128 in ours, 84 distinct types):

| Source | Pairs in source | Overlap | Ours-only |
|---|---|---|---|
| h5bp server-configs-nginx (MIT) | 144 | 112 | 16 |
| nginx `conf/mime.types` (BSD-2) | 107 | 90 | 38 |
| Apache httpd (Apache-2.0) | 1000 | 74 | 54 |
| IANA registry (names only, no extensions) | 2380 types | 47 of 84 types | 37 types |

68 of ours' 100 non-blank lines are byte-identical to an h5bp line, including the section comments and column alignment. The 16 h5bp misses are the owner's three additions plus renames and regrouping (`application/xml` atom/rdf/rss, `font/opentype`, `application/font-woff*`, `x-font-ttf`, `wmv`, `vcf`, `manifest`). Verdict: mixed, mostly derived from h5bp (MIT), itself an nginx mime.types derivative (BSD-2), with the owner's three entries on top. Credit owed: h5bp's MIT copyright line, and nginx's BSD-2 notice. IANA supplies only names, so nothing is owed; Apache's table overlaps only through shared facts.

### A17 follow-up: can the original lists stay?

Compared with Lucene's analysis-common resources (Apache-2.0; `ca/`, `eu/`, `hy/`, `ro/`, `tr/stopwords.txt` and `snowball/*_stop.txt`), stopwords-iso (MIT, Gene Diaz; an aggregate of about 35 web sources listed in its `CREDITS.md`, each with its own unstated terms), NLTK and Alir3z4/stop-words (CC-BY-4.0). Counts are ours / overlap with that source. Xapian's lists were not compared for these languages (the fetch was not permitted); only English was, above.

| Lang | Matches | Evidence | Credit line |
|---|---|---|---|
| ca | Lucene `ca/stopwords.txt`, exact | 218 of 218; header says from vcl/cue.language (Apache-2.0) | Apache-2.0 notice naming vcl/cue.language and Lucene |
| eu | Lucene `eu/` exact | 98 of 98; "example set" header, Apache-2.0 via Lucene | Lucene Apache-2.0 notice |
| hy | Lucene `hy/` exact | 45 of 45; same | Lucene Apache-2.0 notice |
| no | Snowball Norwegian exact (= Lucene, NLTK) | 172 of 172 | Snowball BSD-3 notice |
| sv | Snowball Swedish exact for 114 words, plus 287 more; equals Alir3z4 (401 of 401) | the extra words come from an aggregate | Snowball BSD-3, source of the rest unrecorded |
| ro | Lucene `ro/` (Jacques Savoy, BSD), partial | ours 282, Lucene 254, overlap 228 | Savoy BSD notice (copyright line from Lucene's header) |
| tr | Lucene `tr/` (LUCENE-559 plus a JASIST 2008 paper list) is a subset of ours | ours 279, Lucene 209, overlap 209; 70 extra | Lucene Apache-2.0 notice; the 70 extra words have no source |
| it, hu, ru, nl, de | Snowball is a (near) subset of ours: it 279/279, hu 197/198, ru 158/159, nl 90/101, de 208/231 | extras from an aggregate (stopwords-iso contains all of ours except de 2, it 29) | Snowball BSD-3; extras unsourced |
| da, es, fi, fr, pt | Snowball overlaps only partly (da 50/94, es 166/308, fi 156/229, fr 101/154, pt 119/203) | stopwords-iso contains every word of ours (stopwords-iso is a superset) | no single upstream identified |
| en | SMART, exact | see below | see below |

Finding: no list is the owner's curation and none has a recorded upstream; at most 5 (ca, eu, hy, no, plus ro and tr in part) match a licensed upstream exactly or as a core. For da, es, fi, fr, pt and the extras in the others, stopwords-iso contains them, so they derive from public web lists (probably ranks.nl, the Snowball family and similar, per `CREDITS.md`), whose terms cannot be established. Those can stay only with a generic credit line that is honest about that. Cleanest: ca, eu, hy, ro, tr, no from Lucene/Snowball with their notices; da, de, es, fi, fr, hu, it, nl, pt, ru, sv regenerated from Snowball's BSD-3 lists (Lucene carries them too).

SMART (`en.txt`): the web has no license text for the original. The list is credited to Gerard Salton and Chris Buckley, SMART system, Cornell, 1971; copies cite `ftp://ftp.cs.cornell.edu/pub/smart/english.stop` and carry no terms (RAKE's `SmartStoplist.txt` cites Salton 1971 and the FTP path only; R `stopwords` (MIT) and stopwords-json (Apache-2.0) bundle it under their own license without relicensing it). The FTP site did not answer, so the SMART package's README could not be read. Verdict: there is no known license with conditions, nor a grant; the practice is an attribution line only. Keep it with "SMART stop list, Salton and Buckley, Cornell University" in the notice, or swap to Snowball English (BSD-3) to remove the doubt. The owner decides.

## Actions

1. **A1**: Rewrite `NOTICE` as the third-party notice file.
2. **A2**: Rewrite `site/src/content/docs/copyright.md` to match.
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
24. **A24**: Set `"license": "MIT"` in `site/package.json`.
25. **A25**: Install `LICENSE` and `NOTICE` with the binary.
26. **A26**: Decide the package license metadata.
27. **A27**: Re-run the audit at the release commit.

## Ledger: license actions (branch `phase8/license-actions`)

State (2026-11-03): A1, A2, A4, A5, A6, A7, A8, A10, A11, A12, A13, A14, A15, A16, A20, A22, A23, A24 and A25 are done on the branch; the owner's actions (A3, A9, A17, A18, A19, A21, A26, A27) and the three blockers are untouched. Commit hashes carry the repository: `Xapiand:`.

| Action | Status | Evidence |
|---|---|---|
| A1 | done | `NOTICE:1-26` (owner 2026, Dubalu 2015-2020, MIT pointer, Elasticsearch section); `THIRD-PARTY-NOTICES` generated by `cmake/generate_third_party_notices.py` from the pinned `_deps`; rerun against a fresh configure at master's pins reproduces it byte for byte. Mechanism note `docs/mechanisms/third-party-notices.md`. The stopword and mime.types sources are not in it (A17, A18 are the owner's). |
| A2 | done | `site/src/content/docs/copyright.md:6-31` matches `NOTICE`. |
| A4 | done | `AUTHORS:1` is `german.mb@gmail.com`. |
| A5 | done | `git grep -E 'Copyright \(c\) 2026 Dubalu'` matches only this document; `src/engine/search.cc:2`, `src/exception_engine.h:2`, `tests/fuzz_body.cc:2` carry the owner's line. |
| A6 | done | 17 `src/` files (`src/tls.cc:2`, `src/debounce_or_run.h:2`, `src/xapiand_*_trace.h:2`, ...) and `tests/lua_patch_drift.cmake:1` carry the 2026 MIT header. |
| A7 | done, with exemptions | Headers on 57 `harness/` scripts (`harness/env.sh:1`), the Dockerfile, `contrib/homebrew/xapiand.rb`, `contrib/lua/lua-version.cmake`, `cmake/Modules/GenPackageHash.cmake`, `site/astro.config.mjs`, `site/src/{consts,content.config,starlightRouteData}.ts`, `site/src/lib/{normalizeAnsi,renderD2}.mjs`, `site/src/plugins/*.mjs` and 9 `site/src/components/*.astro` (inside the frontmatter, so nothing renders). Seven of the components are Starlight overrides (`Footer`, `PageTitle`, `Pagination`, `Sidebar`, `SidebarSublist`, `SiteTitle`, `SocialIcons`) and also name Starlight's MIT copyright (2023 Astro contributors). Exempt: `harness/{README.md,ruff.toml,pyrefly.toml,async_allowlist.tsv,.gitignore}` (data and config), `site/src/lib/sidebar.mjs` (generated), `docs/perf/t11-barriers/raw/` (evidence kept byte for byte). |
| A8 | done | `config.h.in:2-3`, `package_config.h.in`, `cmake/cmake_uninstall.cmake.in`, `cmake/Modules/{RemoveDuplicateSubstrings,AC_HEADER_STDC}.cmake`, `docs_to_postman.py`, `contrib/docker/xapiand/entrypoint.sh`, `contrib/python/{multicast.py,cuuid/mertwis.py}`, `site/public/assets/{generator,update_formats}.py`, `contrib/python/xapiand-py/setup.py` carry owner 2026 plus Dubalu 2015-2020. `RemoveDuplicateSubstrings.cmake`'s copied AddCXXCompilerFlag comment is rewritten. `contrib/python/xapiand-py/MANIFEST.in` is a data file, exempt. `setup.py` declares Apache-2.0 for the client as a whole; its new header names only the MIT parts' owners, which the client's other files already do. |
| A10 | done | `NOTICE:20-22`, `site/src/content/docs/copyright.md:25-28`, and the generated `THIRD-PARTY-NOTICES` name `contrib/python/xapiand-py`, its Apache-2.0 `LICENSE` and Copyright (c) 2017 Elasticsearch, and that no package carries it. |
| A11 | done | `contrib/javascript/cuuid/src/bx.js:11-34` has bs58's MIT text with Google Inc 2011 and BitPay 2013. |
| A12 | done | `contrib/geospatial/google_map_plotter.py:14` has the MIT text and "Copyright (c) 2014-2019 Michael Woods", checked against gmplot's own LICENSE (`github.com/gmplot/gmplot`, which adds a 2020 line for later contributors that this older copy does not carry). |
| A13 | done, confirmed | The 2019 import commit `Xapiand:7fe1e367f` names `github.com/tangrams/y2j` at 45396f91. The upstream repository (checked 2026-11-03) is MIT, has `y2j.h` declaring `yamlParseBytes`, and an MIT `LICENSE` "Copyright (c) 2016 Mapzen"; `src/y2j/LICENSE:3` says 2016,2019 and stays as imported. `src/y2j/y2j.h:1-8` and `src/y2j/y2j.cc:1-8` now name the origin. The generated notice includes `src/y2j/LICENSE`. |
| A14 | done | `cmake/Modules/AddCXXCompilerFlag.cmake:1-5`, `cmake/Modules/GetGitVersion.cmake:1-5` name google/benchmark and Apache-2.0 (upstream's LICENSE has no copyright line, so none is given). |
| A15 | done | `cmake/Modules/CheckPrototypeExists.cmake:1-23` has KDE's BSD-3 text; the copyright line "Copyright (c) 2006, Alexander Neundorf" is the one kdelibs' file carries. |
| A16 | done by deletion | `cmake/Modules/CheckDIRSymbolExists.cmake` is gone; `cmake/Modules/AC_HEADER_STDC.cmake:56` runs `check_c_source_compiles` with the same test program; configure reports `HAVE_DIRENT_H - Success`. |
| A20 | done | `site/public/assets/htm.png` (1800x1152) is a raster of `htm.svg` (the owner's Illustrator export): both arrive together in `Xapiand:d7b41caa4` (2019-02-20, the HTM docs commit), the aspect ratios match (1.5625 and 1.5617), and rendering the SVG with `rsvg-convert` shows the same five tessellated spheres as the PNG. Source: the owner's own artwork. |
| A22 | done | `NOTICE:24-26` and `copyright.md:30-31` keep the Elasticsearch documentation attribution. |
| A23 | done | Each repository's `LICENSE` ends with "Third-party notices": `allocators:0029140`, `compressors:5566260`, `enum-reflection:a360207`, `boolean-parser:f212b8c`, `cuuid:1df1c06`, `hashes:5791db9`, `scheduler:f080858`, pushed to each `main`. Latest CI at the time of the push: green on compressors, enum-reflection, boolean-parser, cuuid, hashes and scheduler before it; allocators' last scheduled nightly (2026-11-03 15:21Z, sha 07de45d) failed although the run on the same sha the day before passed, so that failure predates the commit. The pins in `CMakeLists.txt` are not moved. |
| A24 | done | `site/package.json:6` and the root entry of `site/package-lock.json` say `"license": "MIT"`. |
| A25 | done | `CMakeLists.txt:1309-1310` installs `LICENSE`, `NOTICE` and `THIRD-PARTY-NOTICES` into `share/doc/xapiand`. |

Build and test: a Release build (Ninja, `BUILD_TESTS=ON`, `LTO=OFF`, ccache) of the branch at `Xapiand:6a9218ef2` finished with exit 0, `ctest -j4` passed 37 of 37, and `cmake --install` staged `share/doc/xapiand/{LICENSE,NOTICE,THIRD-PARTY-NOTICES}`. The changes are comments, notice files and one CMake check (A16), so no mutant or revert test applies; a configure with the A16 replacement reported `HAVE_DIRENT_H - Success`. No fuzz target applies (no parser or decoder changed).

Not done, by instruction: A3 (waits for `sortable_serialise`'s removal), A9 (`docs/discussions` must stay byte-identical with two other copies), A17, A18, A19, A21, A26, A27. `site/public/assets/Lenna.*` (blocker 3) is untouched on this branch; an earlier local branch `fix/license-actions` (never pushed, worktree `Xapiand-wt/license-actions`) also replaces those images with a generated sample, which this branch left out.

Design vs code: the audit asks for A1 as a rewritten `NOTICE` that carries the third-party texts; the code splits it into `NOTICE` (ownership, short) and a generated `THIRD-PARTY-NOTICES` (texts), both installed. No other differences.

## Ledger: license closure (quest `license-closure`)

State (2026-11-16, branch `quest/xapiand/license-closure`): blocker 3, A3, A18, A19, A21, A26, the RapidJSON and Lua rows and the Xapian scan are closed. Commit hashes carry the repository: `Xapiand:`.

| Item | Status | Evidence |
|---|---|---|
| Blocker 3 | done | `site/public/assets/Lenna.*` are gone; `site/public/assets/sample.{png,jpg,pdf}` come from `sample_image.py` (a generated interference pattern) and `store-api.md` uses them, with lengths and SHA-256 digests that match the files. No tracked file outside history documents names the old files (`git grep -i lenna`). |
| A3 | done | `ARCHITECTURE.md:609-614` (the licensing note), `README.md:124-129` and `AGENTS.md:42,104` call the value encoding the MIT `Kronuz/sortable`; no tracked file outside design history and the changelog says `sortable_serialise` is GPL, and `src/sortable_serialise.*` and the Python `sortable.py` port are gone. |
| A9 | done, owner's ruling | `docs/discussions` is the owner's own code and fully MIT: `docs/discussions/LICENSE` and its README line here, and the same two files in the personal blog copy (`Kronuz.github.io:afd643d`). The `gmendezb-pages` copy is in a work repository, outside this quest; the owner handles it. |
| A17 | closed: kept with credits (D97) | The 18 `stopwords/*.txt` lists stay exactly as on master. `THIRD-PARTY-NOTICES` (written by `cmake/generate_third_party_notices.py`) carries one entry per source with its license text: Lucene (Apache-2.0) for `ca`, `eu`, `hy` and the base of `tr`; Jacques Savoy's BSD via Lucene for `ro`; Snowball (BSD-3) for `no` and the base of `de`, `hu`, `it`, `nl`, `ru`, `sv`; stopwords-iso (MIT) for `da`, `es`, `fi`, `fr`, `pt`; SMART (Salton and Buckley, Cornell) by attribution for `en`. Words beyond those bases have no recorded source (see "A17 follow-up"). |
| A18 | done, D98 | `mime.types` stays the owner's 2019 file. It is mostly h5bp server-configs-nginx (MIT) lines, themselves from nginx's `mime.types` (BSD-2-Clause), plus the owner's `yaml`, `ndjson` and `msgpack` entries; `THIRD-PARTY-NOTICES` credits both with their license texts. An earlier regenerated copy was dropped. |
| A19 | done, owner's ruling (2026-11-16): keep | `site/public/assets/el_cerrito.png` stays as it is. The trixel overlay is the owner's (his tool drawing Xapiand's HTM code); the basemap is Google Maps under Google's terms, with "Map data ©2019 Google" kept visible in the image. It is a docs-only asset: it is not in the binary and not covered by the MIT source license. |
| A21 | done, fixtures regenerated | The 25 phrase tables of `site/public/assets/generator.py` are rewritten and the fantasynamegenerators.com citation is removed; the first-name, surname and trait-word lists are vocabulary and stay. The lead's ruling (delegated by the owner): the `accounts.*` fixtures are regenerated so no third-party text is left. `generator.py --personality accounts.json` rewrites the 1000 personality texts from the fixed seed `SEED`, keeping every other field and record, and `update_formats.py accounts.json` writes the `.ndjson`, `.http` and `.msgpack` copies; rerunning reproduces the bytes. The docs examples and e2e assertions that counted the old text (the text-queries grammar, simple, phrase, near, love-and-hate, default-operator and stop-words pages) carry the server's exact new numbers; examples whose query no longer matched use a word or phrase the new fixtures contain. |
| A26 | done, owner's ruling | The package license metadata stays plain `MIT`; it is already `MIT` in `.github/workflows/release.yml`, `.github/workflows/freebsd.yml`, `Dockerfile` and `contrib/homebrew/xapiand.rb`, and the third-party notices live in `NOTICE` and `THIRD-PARTY-NOTICES`. |
| RapidJSON's patch | done | The dependency table's RapidJSON and Lua rows above state what each carries: RapidJSON has no patch (one compile definition), Lua has `contrib/lua/xapiand-limits.patch`. |
| Xapian scan | done | `git grep -n -E 'Xapian::|xapian/|"xapian\.h"|<xapian' -- src contrib CMakeLists.txt` matches nothing; the `no-xapian` check runs it. |
| A19 notice | done (review F1) | `NOTICE` and `site/src/content/docs/copyright.md` say `site/public/assets/el_cerrito.png` is a Google Maps screenshot under Google's terms with Xapiand's trixel overlay, and not MIT; `ARCHITECTURE.md`'s licensing note no longer says every file is MIT. |
| `utils/serialize.py` | done (review F2) | `contrib/python/xapiand-py/xapiand/utils/serialize.py` ports the routines of `src/length.{cc,h}`, which are MIT under Olly Betts' grant (`Xapiand:1691e5b2e`); it carries his copyright line and the notice generator names it with `src/length`. The file is in the source release, not in any package. |
| A14b, Kitware | done (review F3) | `cmake/Modules/FindGTest.cmake` and `cmake/Modules/FindPackageTools.cmake` ("Based on official CMake's FindGTest.cmake") carry Kitware's BSD-3-Clause notice in their headers, and `THIRD-PARTY-NOTICES` lists it, as for A14 and A15. Build and test only. |
| `xapiand/sortable.py` | done (review F6) | `contrib/python/xapiand-py/xapiand/sortable.py` has the owner's 2026 MIT header. |
| `docs/discussions` manifests | done (review F7) | `docs/discussions/package.json` and `docs/discussions/worker/package.json` say `"license": "MIT"`; the other copies take the same lines when synced. |
| Brand images | recorded (review F10) | `brand/` (`logo.svg`, the variants, `build-icons.mjs`, `logo-vs-png.png`) is the owner's own work, committed 2026-11-11 as a vector redraw measured from `site/src/assets/logo.png`; `site/public/img/*`, `site/public/{favicon.*,apple-touch-icon*,footer-logo.png}` came with the docs-site migration (`Xapiand:f6299c83c`) and its brand variants (`Xapiand:bf0e44821`), the owner's Xapiand logo and cards. No third-party source is recorded for any of them. |
| `oldtests` polygons | recorded (review F10) | `oldtests/examples/geospatial/convex_hull/{ColoradoPoly,Georgia,Utah}.txt` are state outlines as coordinates (Utah's is a five-point rectangle), added with the 2018 CMake and tests refactor (`Xapiand:86879a90d`); facts, with no source recorded. They are test inputs, not in any package. |
