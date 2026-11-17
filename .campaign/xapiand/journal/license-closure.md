# license-closure: journal

State: waiting on jobs 261116-185812-48bf6d
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

Owner decisions (each is reversible; nothing below was changed unless stated):

1. (Decided, see Rulings.) A3: nothing left for this quest.
2. A4 stopwords: sources unrecorded and mixed (`docs/status/license-audit.md` ledger, A17). Options:
   (a) regenerate from Snowball's BSD-3 lists (fetched; `en` shrinks 570 to 174 words, analysis results change,
   no Snowball list for ro/tr/ca/eu/hy at the URLs tried); (b) record the sources (only you know them; `en` is
   the SMART list); (c) keep and accept. Recommendation: (a) for the nine languages Snowball covers, keep the rest
   until you name their source, as its own quest because tests may pin the words.
3. A4 mime.types: I regenerated it (sorted, one line per type, no comments; extension map identical, 128 entries).
   Confirm that a rewritten arrangement of the same facts clears the nginx/h5bp question, or revert
   (`git revert` of the mime.types hunk of `Xapiand:49f40ee18`) and record nginx BSD-2 and h5bp MIT in `NOTICE`.
4. A5 generator.py fixtures: tables are rewritten in new words; the `accounts.*` fixtures keep the phrases they
   were generated with. Decide whether to regenerate them (changes every account's text and the docs examples
   built on it); recommendation: keep. (`el_cerrito.png` is decided: see Rulings.)
5. (Decided, see Rulings.) A6: nothing left for this quest.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | the e2e check (jobs 261116-152727-4477fa); `docs/public/assets/Lenna.jpg` is removed in `Xapiand:4ecda589a`; `git grep -i lenna` matches only history documents and `.campaign/`, no code, harness or doc example; the store-api lengths and digests match `docs/public/assets/sample.png` (checked); example sizes corrected |
| A2 | `ARCHITECTURE.md` licensing note, `README.md` license section and `AGENTS.md` carry no GPL claim; the sortable-encoding quest already landed on master, so this branch does not touch them (verified by `git grep -i gpl` on the three files, no hit) |
| A3 | [decision] owner's ruling 2026-11-16: `docs/discussions` is fully MIT; `docs/discussions/LICENSE` and `docs/discussions/README.md` here, identical in the blog copy (Kronuz.github.io:afd643d, `cmp` of both files equal) |
| A4 | [decision] Blocked on 2 and 3 (owner); `mime.types` regenerated, stopword comparison in `docs/status/license-audit.md` |
| A5 | [decision] owner's ruling 2026-11-16 on `docs/public/assets/el_cerrito.png`: keep as is (ledger row A19 in `docs/status/license-audit.md`); `docs/public/assets/generator.py` tables rewritten, fixtures pending (Blocked on 4) |
| A6 | [decision] owner's ruling 2026-11-16: package license metadata stays plain `MIT`; the audited metadata already says MIT in `.github/workflows/release.yml` (two places), `.github/workflows/freebsd.yml`, `Dockerfile` and `contrib/homebrew/xapiand.rb`, so no packaging edit; third-party notices in `NOTICE` (new section on Lua's patch and RapidJSON) and `THIRD-PARTY-NOTICES` |
| A7 | the no-xapian check (job 261116-152728-04a92c); the same `git grep` prints nothing locally |
| A8 | `docs/status/license-audit.md`: the RapidJSON row (unmodified upstream, one compile definition), the Lua row (the owner's patch) and the license closure ledger |
| A9 | the build-test check (job 261116-152727-9c07ec) and the DESTDIR install job 261116-185812-48bf6d (stages the three notice files, mime.types and the stopwords) |

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
