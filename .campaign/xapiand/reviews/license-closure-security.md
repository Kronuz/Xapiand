# license-closure review: security (license and provenance for an MIT release)

Commit: Xapiand:70a7398c725966d723e0ecf901342b8fa21de2f2
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The quest's own work holds up: the notice file regenerates byte for byte, the install stages the
notices, the GPL files are gone, the metadata agrees, and the fixtures carry no old phrase. Three
small provenance gaps outside the audit's ledger remain (one wrong sentence, two missing notices);
each is a few lines.

## Evidence I relied on

1. build-test 261116-203022-cfa948 and install 261116-203042-03b690 (passed at Xapiand:c0f8d23c5; the
   only later commits are merges from main and docs, `git diff 2a89a5fa4 70a7398c7 -- CMakeLists.txt
   cmake THIRD-PARTY-NOTICES NOTICE LICENSE stopwords mime.types` changes one test list line). The
   install job's staged tree is what I read for "what `cmake --install` stages".
2. no-xapian 261116-203023-eb84bf: accepted for the source grep; I added the binary check below.

## What I checked myself

Scripts are under `.scratch/review-license-closure-security/` in the home checkout; the three jobs
rerun against the head with `campaign job submit --quest license-closure --level light --minutes 5 -- ...`.

1. Job 261116-211212-b5ea0a (`regen.sh`): regenerates `THIRD-PARTY-NOTICES` with
   `cmake/generate_third_party_notices.py` from the configured `.deps` and diffs it with the committed
   file: identical. Every 40-hex `FetchContent` pin in `CMakeLists.txt` equals the `.deps` checkout
   (no mismatch). So the notice file is the license text of the pinned sources.
2. `deps.sh`, `copyrights.sh`, `prov.sh` (run inline, read-only greps over the configured `.deps` sources):
   every dependency has a license file; the only copyleft hits are lz4's `programs/` and `tests/`
   (GPL-2, not compiled: `CMakeLists.txt:1076` globs `lz4-src/lib/*.c` only), cppcodec's test scripts
   and prose in strata/soundex docs. Embedded third-party code in the Kronuz libraries is exactly the
   generator's `BUNDLED`/`HEADERS` set plus msgpack and static-string (BSL, name only).
   double-metaphone states its own provenance (a fresh implementation); threadpool's
   `concurrent_queue.h` is the owner's shim, not moodycamel.
3. Staged install tree (job 261116-203042-03b690's `build/stage`): `bin/xapiand`, `share/doc/xapiand/{LICENSE,NOTICE,THIRD-PARTY-NOTICES}`,
   `share/xapiand/{mime.types,stopwords/*.txt}`, the man page, and prometheus-cpp's own install
   (`lib/libprometheus-cpp-core.a`, 21 headers, `lib/cmake/prometheus-cpp`). No GPL file. `strings` on
   the staged binary: no `Olly`/`GPL`/license-text strings, and the only Xapian strings are the data-guard
   messages and file names (`src/xapian_data_guard.h`).
4. Job 261116-211458-e2f7da (`fixtures.py`, Q4): of 203 phrases in the old 25 tables (from `git show
   master:docs/public/assets/generator.py`), one survives in the new tables (`People regularly`, two
   common words); no old phrase of three or more words is in `accounts.json`, `.ndjson` or `.http`
   (the old fixture held 186). Job 261116-211611-4d0818 (`fixtures2.py`): the 1000 texts are
   identical across json, ndjson, http and msgpack, and no old phrase is in the msgpack bytes.
   The `good`, `bad`, first-name and surname tables are unchanged from the old generator (the quest's
   ruling: vocabulary).
5. Reads: `NOTICE`, `LICENSE`, `README.md:124-129`, `ARCHITECTURE.md:609-614`, `AGENTS.md`, the docs
   copyright page, `about.md`, `faq.md`, `Footer.astro`, the release/FreeBSD/Docker/Homebrew metadata,
   `docs/package.json`. Plain `MIT` is the same in `.github/workflows/release.yml:181,247`,
   `freebsd.yml:120`, `Dockerfile:65`, `contrib/homebrew/xapiand.rb:40` and `docs/package.json:6`;
   the copyright lines (owner 2026, Dubalu 2015-2020) agree in LICENSE, NOTICE, README, the docs page and the footer.
6. Gone: `src/sortable_serialise.*` and `contrib/python/xapiand-py/xapiand/utils/sortable.py` do not
   exist (checked by path only, never opened); the replacement `xapiand/sortable.py` is a different file.
   `git grep` for GPL / `General Public License` outside history documents finds nothing.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | `ARCHITECTURE.md` says Xapiand "is MIT ... and so is every file in this tree". It is not, and the repository says so elsewhere: `NOTICE` and `copyright.md` name the Apache-2.0 Python client and Elasticsearch-derived docs; `about.md` says "a few files carry their own"; `docs/public/assets/el_cerrito.png` is a Google Maps basemap that no shipped notice mentions (D100 says "credited"; the only credit is the "Map data ©2019 Google" text inside the image); `stopwords/en.txt` is attribution-only. The audit calls the image "not covered by the MIT source license", but nothing a recipient reads says that. | `ARCHITECTURE.md:611`; `NOTICE`; `docs/src/content/docs/copyright.md`; `docs/src/content/docs/about.md:12`; `docs/status/license-audit.md` row A19 | Change the sentence to "MIT, except the files that carry their own notice (see NOTICE and THIRD-PARTY-NOTICES)". Add one line to `NOTICE` and `copyright.md`: `docs/public/assets/el_cerrito.png` is a Google Maps basemap (Map data ©2019 Google) under Google's terms, the trixel overlay is Xapiand's, and the image is not MIT. Keeping the image is the owner's ruling and stays. |
| F2 | yes | gap | `contrib/python/xapiand-py/xapiand/utils/serialize.py` ports the same functions as `src/length.{cc,h}` (`serialize_length`, `unserialize_length(..., check_remaining)`, `serialize_string`, `serialize_char`, the 255-escape 7-bit continuation) but carries only the owner's and Dubalu's lines. `src/length.*` are MIT because Olly Betts granted it for those two files (`Xapiand:1691e5b2e`); his copyright line is the notice that must travel with derivatives, and `THIRD-PARTY-NOTICES` and `NOTICE` name only `src/length.{cc,h}`. The file is in the source release (not in any package). Whether his grant reaches a Python port is a question I cannot settle from the repository. | `contrib/python/xapiand-py/xapiand/utils/serialize.py:31-88` against `src/length.h:36-70`; `THIRD-PARTY-NOTICES:720-723`; `git log --follow` shows the file arrived 2019-03-06 ("Python Client: Added Xapiand utils") | Add `Copyright (c) 2006-2012 Olly Betts` to the file header and name it in the `src/length` entry of the notice generator (`cmake/generate_third_party_notices.py:222`); or, if the owner has no grant for it, replace it with a rewrite from the format description alone (the way `xapiand/sortable.py` was written) or drop the helper. The owner confirms which. |
| F3 | yes | defect | `cmake/Modules/FindPackageTools.cmake` and `cmake/Modules/FindGTest.cmake` say "Based on official CMake's FindGTest.cmake" (Kitware, BSD-3-Clause) with no Kitware copyright or license text. The audit fixed the same class of gap for google/benchmark (A14) and KDE (A15) but missed these. Build and test only; nothing in the binary. | `cmake/Modules/FindPackageTools.cmake:24`, `cmake/Modules/FindGTest.cmake:24`; `docs/status/license-audit.md` rows A14, A15 | Add Kitware's BSD-3-Clause notice to both headers (as `CheckPrototypeExists.cmake:1-23` does for KDE), or rewrite the helpers from scratch and drop the sentence. Add a ledger row. |
| F4 | no | gap | The English stop list is the SMART list with an attribution only (no license text exists), and `da es fi fr pt` plus the extra words of the others have "no recorded source": the stopwords-iso MIT text covers the aggregator, not its sources. `THIRD-PARTY-NOTICES` says both honestly. Kept by the owner's ruling D97. Options: keep (current); or swap `en.txt` for Snowball English (BSD-3, 174 words, changes stop-word counts in the docs) and the five aggregate lists for Snowball's where it ships them. The owner's call. | `THIRD-PARTY-NOTICES:1039-1075`; `docs/status/license-audit.md` A17 follow-up | none required |
| F5 | no | nit | The journal's "Blocked on" paragraph says the fixtures "keep sentences built from the old phrase tables"; its Result and the evidence above say the opposite (no old phrase of three words or more survives). The kept `good`/`bad` trait-pair tables and the name lists are vocabulary from the same source site; the sentence skeleton in `positive_personality`/`negative_personality` also stays. | `.campaign/xapiand/journal/license-closure.md` "Blocked on"; job 261116-211458-e2f7da | Correct the paragraph. If the owner wants the trait tables gone too, regenerate them; not needed for the ruling as worded. |
| F6 | no | nit | `contrib/python/xapiand-py/xapiand/sortable.py` (added by the sortable-encoding quest) has no license header inside a package whose `LICENSE` and `setup.py` are Apache-2.0; it is the owner's MIT code. | `contrib/python/xapiand-py/xapiand/sortable.py:1` | Add the 2026 MIT header like `utils/serialize.py`. |
| F7 | no | nit | `docs/discussions/package.json` and `worker/package.json` have no `license` field although D99 says the directory is MIT (the journal says the copies differ, so left alone). `npm license-checker` will report UNLICENSED for them. | `docs/discussions/package.json:4`, `docs/discussions/worker/package.json:4` | Add `"license": "MIT"` when the copies are synced. |
| F8 | no | nit | Lenna is gone from the tree but stays in git history of the public repository (`git log --all -- docs/public/assets/Lenna.jpg`). A source tarball and a fresh checkout are clean; a clone is not. Whether to rewrite history is the owner's call; the options are to keep (nothing ships it), or to filter it out before the first MIT tag. | `docs/status/license-audit.md` Blocker 3 row | none required |
| F9 | no | nit | `cmake --install` also installs prometheus-cpp's library, 21 headers and CMake config into `lib/` and `include/`, so every deb, rpm and pkg ships them (MIT, covered by its notice entry). Likely unintended in the packages. | staged tree of job 261116-203042-03b690 | Exclude prometheus-cpp's install rules from the top-level install, or package only the `xapiand` target. Packaging, not a license matter. |
| F10 | no | nit | Unrecorded provenance, owner's own or facts, not in the ledger: `oldtests/examples/geospatial/convex_hull/{ColoradoPoly,Georgia,Utah}.txt` (state outline coordinates, 140 KB), and the brand images (`brand/`, `docs/public/img/*`, `docs/public/{favicon.*,apple-touch-icon*,footer-logo.png}`). `src/*` bm25_xapian is a scheme name for Xapian's published BM25 parameters (no Xapian code); keep or rename at the owner's choice. | `git ls-files oldtests brand docs/public` | One ledger line each if the owner wants the closure complete. |

## Not run

1. The Kronuz libraries were scanned for embedded third-party code and provenance statements by
   headers and grep, not read file by file; the libraries are outside this quest's tree.
2. No network: I did not re-check upstream licenses (Lucene, Snowball, stopwords-iso, h5bp, nginx,
   Kitware); I compared the texts the notice file carries with the sources' license files in `.deps`
   (identical, job 261116-211212-b5ea0a) and with the audit's recorded provenance.
3. The Elasticsearch documentation's license (Apache-2.0 at the time of the import, per `NOTICE`) is
   taken from the repository's own statement; the imported revision is not named anywhere.
4. System libraries (OpenSSL 3, zlib, zstd, libuuid, libcap) are dynamic package dependencies; I read
   how the packages declare them (`release.yml`, `Dockerfile`), not their copyright files.
5. No GPL file was opened, and Xapian's sources were not read; the F2 comparison is between two files of this repository.
6. CI at 70a7398c7 (FreeBSD, CI, arm64) was still running when I stopped.

## Round 2

Commit: Xapiand:90454dc6d533820c3cf5edf4e48b8094f3e4059b (journal-only commit be9bb3331 on top)
Verdict: APPROVE
Date: 2026-11-16

Checked in the diff `bb59b0e74..90454dc6d` and by rerunning `regen.sh` (job 261116-212240-84e046: notice file regenerates identically at the new head, pins match). build-test 261116-212229-7bfdbe passed at this commit.

| ID | Status | Evidence |
|---|---|---|
| F1 | fixed | `ARCHITECTURE.md:612-613` now says "except the files that carry their own notice (named in `NOTICE` and `THIRD-PARTY-NOTICES`)"; `NOTICE` and `copyright.md` have a "Google Maps" section saying `el_cerrito.png` is a Google Maps screenshot under Google's terms, the overlay is Xapiand's, and the image is not MIT; ledger row "A19 notice". |
| F2 | fixed, on the lead's ruling | `contrib/python/xapiand-py/xapiand/utils/serialize.py:3` carries Olly Betts' line; `cmake/generate_third_party_notices.py` names the Python port in the `src/length` entry (`THIRD-PARTY-NOTICES:723`). The ruling that his grant for `src/length.*` reaches the port is the lead's, not something I could verify. |
| F3 | fixed | `FindGTest.cmake` and `FindPackageTools.cmake` headers carry Kitware's BSD-3 text; `THIRD-PARTY-NOTICES:731-744` lists it (one entry for both). The copyright year range "2000-2026" is the lead's wording; upstream's own line was not re-checked (no network). |
| F4 | open, owner's | unchanged (lead's). |
| F5 | fixed | the journal's "Blocked on" no longer claims old phrases remain. |
| F6 | fixed | `xapiand/sortable.py:1-20` has the 2026 MIT header. |
| F7 | fixed | `"license": "MIT"` in both discussions `package.json` files. |
| F8, F9 | open, lead's | unchanged. |
| F10 | fixed | ledger rows for `brand/`, the docs images and the `oldtests` polygons in `docs/status/license-audit.md`. |

No new blocking finding.
