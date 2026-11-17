# license-closure: journal

State: waiting on jobs 261116-191254-7eda99 261116-191255-64e1c6 261116-191255-aaf381 and the A9 install job; next: the e2e failures give the exact doc counts the regenerated fixtures moved; set them, no loosening
Quest: .campaign/xapiand/quests/license-closure.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Rebased onto master (four of five commits had landed; `Xapiand:4ecda589a` is the Lenna replacement), then
`Xapiand:49f40ee18`: generator.py phrase tables, mime.types, docs/discussions LICENSE, audit ledger. Jobs queued
at that commit (A1 e2e, A7 no-xapian, A9 build-test and install staging).

## Next

1. `campaign wait` the four jobs; paste `campaign brief`'s table in Evidence; set "ready for review" if green.
2. Owner answers the Blocked on items; apply them.

## Blocked on

Nothing. The accounts fixtures keep sentences built from the old phrase tables (fantasynamegenerators.com, no
license); they stay as the owner's test data under the lead's ruling, and the lead is told in the report.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | the e2e check (jobs 261116-152727-4477fa); `docs/public/assets/Lenna.jpg` is removed in `Xapiand:4ecda589a`; `git grep -i lenna` matches only history documents and `.campaign/`, no code, harness or doc example; the store-api lengths and digests match `docs/public/assets/sample.png` (checked); example sizes corrected |
| A2 | `ARCHITECTURE.md` licensing note, `README.md` license section and `AGENTS.md` carry no GPL claim; the sortable-encoding quest already landed on master, so this branch does not touch them (verified by `git grep -i gpl` on the three files, no hit) |
| A3 | [decision] owner's ruling 2026-11-16: `docs/discussions` is fully MIT; `docs/discussions/LICENSE` and `docs/discussions/README.md` here, identical in the blog copy (Kronuz.github.io:afd643d, `cmp` of both files equal) |
| A4 | [decision] D98 (the owner's `mime.types` kept, h5bp MIT and nginx BSD-2 credited) and D97 as revised (all 18 stopword lists kept as on master with one `THIRD-PARTY-NOTICES` entry per source and its license text): `mime.types`, `stopwords/`, `THIRD-PARTY-NOTICES`, `docs/status/license-audit.md` rows A17 and A18 |
| A5 | [decision] the lead's ruling on `docs/public/assets/el_cerrito.png` (kept, credit visible; ledger row A19); `docs/public/assets/generator.py` tables rewritten; the fixtures stay as the owner's test data (row A21) |
| A6 | [decision] the owner's ruling: plain `MIT` metadata (ledger row A26); `NOTICE` carries the notices |
| A7 | the no-xapian check (job 261116-152728-04a92c); the same `git grep` prints nothing locally |
| A8 | `docs/status/license-audit.md`: the RapidJSON row (unmodified upstream, one compile definition), the Lua row (the owner's patch) and the license closure ledger |
| A9 | the build-test check (job 261116-191254-7eda99) and the DESTDIR install job after it (notice files, mime.types, stopwords staged) |

## Result

Not yet: waiting on jobs. No code changed; the diff is docs, `mime.types`, a docs asset script and the
discussions license. Mechanism notes: none new (`docs/mechanisms/third-party-notices.md` unchanged).

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
