# license-closure: journal

State: ready for review (master merged at 0ae4f966d; checks reused from c0f8d23c5f41, passed: build-test cfa948, e2e d1015e, no-xapian eb84bf, install 03b690)
Quest: .campaign/xapiand/quests/license-closure.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing; ready for review.

## Next

1. Security review, then land.

## Blocked on

Nothing. (The review's F4 stopword terms, F8 Lenna in git history and F9 the prometheus-cpp install are the lead's.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | the e2e check (job 261116-203023-d1015e, at the head with the fixtures' exact doc numbers); `docs/public/assets/Lenna.jpg` is removed in `Xapiand:4ecda589a`; `git grep -i lenna` matches only history documents and `.campaign/`; the store-api lengths and digests match `docs/public/assets/sample.png` (checked) |
| A2 | `ARCHITECTURE.md` licensing note, `README.md` license section and `AGENTS.md` carry no GPL claim; the sortable-encoding quest already landed on master, so this branch does not touch them (verified by `git grep -i gpl` on the three files, no hit) |
| A3 | D99 (`docs/discussions` fully MIT): `docs/discussions/LICENSE`, identical in the blog copy (Kronuz.github.io:afd643d); the work-repo copy is the owner's |
| A4 | [decision] D98 (the owner's `mime.types` kept, h5bp MIT and nginx BSD-2 credited) and D97 as revised (all 18 stopword lists kept as on master with one `THIRD-PARTY-NOTICES` entry per source and its license text): `mime.types`, `stopwords/`, `THIRD-PARTY-NOTICES`, `docs/status/license-audit.md` rows A17 and A18 |
| A5 | D100 (`docs/public/assets/el_cerrito.png` kept and credited; the accounts fixtures regenerated, `docs/public/assets/generator.py` rewritten): audit ledger rows A19 and A21; e2e 261116-203023-d1015e passes on the new fixtures |
| A6 | D101 (package license metadata plain `MIT`): `NOTICE`, `docs/status/license-audit.md` row A26 |
| A7 | 261116-203023-eb84bf (the no-xapian check) |
| A8 | `docs/status/license-audit.md`: the RapidJSON row (unmodified upstream, one compile definition), the Lua row (the owner's patch) and the license closure ledger |
| A9 | the build-test check (261116-203022-cfa948) and install job 261116-203042-03b690 (notice files, mime.types, stopwords staged) |

## Result

Blocker 3 and the owner's actions are closed, with the rulings D97 (revised), D98 to D101: the generated sample replaces Lenna; mime.types is kept and credited (h5bp MIT, nginx BSD-2); the 18 stopword lists stay with one credit entry per source in `THIRD-PARTY-NOTICES` (Lucene, Savoy, Snowball, stopwords-iso, SMART); `docs/discussions` carries MIT; `el_cerrito.png` is kept with its credit; the accounts personality texts are regenerated from the rewritten `generator.py` (fixed seed) with the text-query docs set to exact new numbers; package metadata stays `MIT`; `NOTICE` notes Lua's patch and RapidJSON. The audit's ledger (`docs/status/license-audit.md`) and `docs/mechanisms/third-party-notices.md` record it. Jobs: build-test 261116-203022-cfa948, e2e 261116-203023-d1015e, no-xapian 261116-203023-eb84bf, install 261116-203042-03b690. Open: nothing for this quest (A27, the audit re-run at the release commit, belongs to `release`).

## Rulings I made

+ mime.types regenerated rather than recorded: behavior-identical (loader's rules, map compared), reversible.
+ generator.py: only the 25 phrase tables rewritten; names and trait vocabulary lists kept (facts/vocabulary);
  the cited fantasynamegenerators.com docstring line removed; template strings unchanged.
+ docs/discussions gets MIT with only the owner's 2026 line (all 2026 work); no `package.json` change because the
  copies differ there.
+ The audit's RapidJSON "patch" is a compile definition plus scratch-only sanitizer patches to the old pin; Lua's
  patch is the only patched source. Recorded as such.
+ store-api example sizes updated; the offsets in that illustrative JSON are left as they were.
+ Owner's ruling (2026-11-16, via the lead): `el_cerrito.png` is kept unchanged. The trixel overlay is the owner's (his tool, Xapiand's HTM code); the basemap is Google Maps under Google's terms with "Map data ©2019 Google" visible; docs-only asset, outside the binary and the MIT source license. An earlier ruling to redraw it was reversed before any redraw work was committed.
+ Owner's ruling (2026-11-16, via the lead): `docs/discussions` is the owner's own code and fully MIT. The lead added the same LICENSE and README to the blog copy (Kronuz.github.io:afd643d). The `gmendezb-pages` copy is in a work repository, outside this quest; the owner handles it.
+ Owner's ruling (2026-11-16, via the lead): A6 package license metadata is plain `MIT`; third-party notices (Lua's patch, RapidJSON, the rest) live in the root `NOTICE`. The metadata is already MIT in every package file, so nothing changed there; `NOTICE` and `docs/src/content/docs/copyright.md` gained a short "Modified and configured third-party code" section. I did not edit `docs/status/license-audit.md` for this (another contributor writes it); its A26 ledger row still says the owner decides, to update when that contributor reports.
+ A9 install check: the install rules were right; my check was wrong. `XAPIAND_DATA_PATH` is the absolute `/usr/local/share/xapiand`, so `cmake --install --prefix` ignores the prefix for mime.types and stopwords and wrote them to the real `/usr/local/share/xapiand` (same content as the tree's: `cmp` equal). The check now uses `DESTDIR`, which stages under `build/stage/usr/local/share`. CMakeLists.txt needs no change.
+ D97 (lead, owner-delegated): stopwords are Snowball's (source `snowball-website` at `dc14fb462784`, BSD-3) for the 13 languages already present that Snowball ships (da de en es fi fr hu it nl no pt ru sv); I did not add Snowball's other languages (indonesian, irish, sesotho, ukrainian) since no field uses them here. ca eu hy ro tr dropped. Lists are one word per line because `src/stopper.cc` reads whitespace tokens (Snowball's `|` comments would become words). Docs: `far` is no longer an English stop word, so the stop-words and phrase pages are corrected.
+ D98: `mime.types` restored from master (`git diff master -- mime.types` is empty); regenerated copy dropped; credits are static text in `cmake/generate_third_party_notices.py`, regenerated with the slot's `.deps` (the old output was reproduced byte for byte first).
+ Fixtures ruling (lead, delegated by the owner): the accounts fixtures are regenerated with the rewritten `generator.py` so no third-party text remains. I regenerate only the personality text (`generator.py --personality`, fixed `SEED`; same 1000 records, names and fields), because a full regeneration would change every name, id and coordinate the docs examples cite. Docs and e2e numbers that depended on the text are set to the exact new values from a local docs e2e run (no assertion loosened).
+ D97 reopened (lead, owner): restored `stopwords/` to master's 18 lists, removed the Snowball stopword section from `cmake/generate_third_party_notices.py` and `THIRD-PARTY-NOTICES`, and reverted the `far` wording in the stop-words and phrase pages. The earlier Snowball entry above is superseded. A17 stays open until the provenance check reports. Stop-word e2e counts are unchanged from master (they are still the old values; fixtures regeneration may move them).
+ D97 revised (lead, 5093d680c): the 18 lists stay as on master; `THIRD-PARTY-NOTICES` gets Lucene (Apache-2.0, with the full text from `prospect-src/LICENSES`), Savoy BSD (text from Lucene's stemmer headers), Snowball BSD-3, stopwords-iso MIT and the SMART attribution; audit A17 closed. Stop-word e2e counts stay at master's. I will not kill processes by pattern again (my earlier `pkill -f e2e_capture.sh` was machine-wide).
+ Fixture numbers: the 16 failing e2e assertions (text-queries grammar x5, simple, phrase x4, near x3, love-and-hate, default-operator, stop-words) were all the regenerated personality text; stopwords and mime.types are master's. I probed a local node with the fixtures (my own PID, killed by it) and set each number and id list to the server's exact output, no loosening. Examples whose words vanished changed query: `adventurous creative` replaces `ambitious adventurous` (the grammar and near pages), the phrase `these come alongside a habit of` replaces `these days are few and far between` on the phrase page, and the simple-query prose names the stem family (responsiveness, responsible, responsibility). `exploration.md`'s sample document starts with the new text.
+ Security review CHANGES (`Xapiand:bb59b0e74`): fixed F1 (ARCHITECTURE wording, NOTICE and copyright.md line for el_cerrito.png), F2 (Olly Betts line on `utils/serialize.py` and in the notice generator, per the lead's ruling), F3 (Kitware BSD-3 in `FindGTest.cmake` and `FindPackageTools.cmake`, in the notices and the ledger), F5 (the stale Blocked on), F6 (MIT header on `xapiand/sortable.py`), F7 (`license` in both docs/discussions package.json files), F10 (ledger rows for the brand images and the oldtests polygons). All inside the file set.
