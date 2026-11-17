# license-closure: journal

State: waiting on jobs 261116-152727-9c07ec 261116-152727-4477fa 261116-152728-04a92c 261116-152739-cb18f2
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

1. A3 row (docs/discussions): MIT, same text as the root `LICENSE` minus the Dubalu line, is added to this copy.
   Decide MIT or another license, then copy `docs/discussions/LICENSE` and `README.md` into
   `~/code/KronuzBlog/Kronuz.github.io/discussions` and `~/code/KronuzBlog/gmendezb-pages/discussions`
   (other repositories; I did not touch them) and run `scripts/check-copies.sh`. Note the three copies already
   differ in `worker/package.json` and `worker/package-lock.json`. Recommendation: MIT.
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
5. A6 package metadata (`.github/workflows/release.yml:181,247`, `freebsd.yml:98`, `Dockerfile:65`,
   `contrib/homebrew/xapiand.rb:40`; none in my file set): keep `MIT`, or the SPDX expression
   `MIT AND BSD-2-Clause AND BSD-3-Clause AND BSL-1.0 AND Zlib AND Apache-2.0 AND Unicode-3.0` (Homebrew:
   `all_of`). Recommendation: the expression for deb, rpm and Homebrew; Docker label keeps MIT plus the notice
   files. Needs a lead edit outside this quest's files.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | the e2e check (jobs 261116-152727-4477fa); `docs/public/assets/Lenna.jpg` is removed in `Xapiand:4ecda589a`; `git grep -i lenna` matches only history documents and `.campaign/`, no code, harness or doc example; the store-api lengths and digests match `docs/public/assets/sample.png` (checked); example sizes corrected |
| A2 | `ARCHITECTURE.md` licensing note, `README.md` license section and `AGENTS.md` carry no GPL claim; the sortable-encoding quest already landed on master, so this branch does not touch them (verified by `git grep -i gpl` on the three files, no hit) |
| A3 | [decision] Blocked on 1 (owner); prepared `docs/discussions/LICENSE` |
| A4 | [decision] Blocked on 2 and 3 (owner); `mime.types` regenerated, stopword comparison in `docs/status/license-audit.md` |
| A5 | [decision] owner's ruling 2026-11-16 on `docs/public/assets/el_cerrito.png`: keep as is (ledger row A19 in `docs/status/license-audit.md`); `docs/public/assets/generator.py` tables rewritten, fixtures pending (Blocked on 4) |
| A6 | [decision] Blocked on 5 (owner) |
| A7 | the no-xapian check (job 261116-152728-04a92c); the same `git grep` prints nothing locally |
| A8 | `docs/status/license-audit.md`: the RapidJSON row (unmodified upstream, one compile definition), the Lua row (the owner's patch) and the license closure ledger |
| A9 | the build-test check (job 261116-152727-9c07ec) and install job 261116-152739-cb18f2 |

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
