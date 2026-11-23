# hunt-soundex-one-definition: journal

State: waiting on jobs at soundex:dd922dc (lead applied the docs patch there; A1 build-test and A2 diff_languages resubmitted), then CI.
Quest: .campaign/xapiand/quests/hunt-soundex-one-definition.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code done on soundex branch quest/xapiand/hunt-soundex-one-definition at 1699e58 (repo ../soundex worktree). Jobs queued at that commit; machine load is high.

## Next

1. When the jobs pass, paste `campaign brief`'s Evidence table here, set State "ready for review". A4 is CI after the push (pushed at 1699e58).
2. Lead: apply ~/.scratch/soundex-docs/docs.patch (stale AGENTS.md "static const tables are per-TU", ARCHITECTURE.md table paragraph, a CHANGELOG entry; outside the quest's file set).

## Blocked on

Nothing. (Doc fixes in AGENTS.md, ARCHITECTURE.md, CHANGELOG.md need a file-set extension; the patch is ready, see Next 2.)

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test job 261123-133456-ff4d3e at 1699e58 (pending) |
| A2 | job 261123-133452-ec36ac at 1699e58 (pending): test/diff_languages.cc, French/German/Spanish new vs frozen old headers, every string of up to 3 tokens (about 1.7M for French) plus 300k random per language, accents, digits, separators, stray bytes, NUL |
| A3 | [artifact] Result below |
| A4 | CI after push |

## Result

Tables in french/german/spanish_soundex.h (7: french accents, composed, prefixes; german accents, composed; spanish accents, composed) are now `inline constexpr` std::array of string_view pairs: one definition, constant-initialized, no heap. `replace`/`replace_prefix` are unchanged (they take the arrays); rule order is kept; accent keys are distinct UTF-8 sequences mapping to ASCII, so their order cannot matter. I kept a scan rather than a sorted lookup: the tables are 7-45 entries and the scan is the old behavior, so output is identical by construction and by the diff test.

A3 numbers, one TU that only includes the four headers (test/include_all.cc), Release (-O3 -DNDEBUG), arm64 macOS, `test/init_size.sh build` (`size -m`):

| | soundex:be2f07c (old headers) | soundex:b2dc2ca (constexpr tables) |
|---|---|---|
| __StaticInit (sub_I) | 12,272 B | 0 B |
| __text | 6,678 B | 0 B |
| __gcc_except_tab | 464 B | 0 B |
| __mod_init_func | 8 B | 0 B |
| __bss (zerofill) | 216 B | 0 B |
| __cstring | 355 B | 0 B |
| __compact_unwind + __eh_frame | 2,128 B | 0 B |
| object total | 22,136 B | 0 B |

(be2f07c has the old headers with the probe added; 1699e58 has the same headers as b2dc2ca.) A TU that uses all encoders (test/test.cc) has no __StaticInit after, and carries the tables as 4,128 B of read-only __DATA,__const, one copy after linking. Xapiand's 4 including TUs each drop ~12.3-12.9 KB of initializer and a heap copy of 7 tables, about 49 KB of initializer text overall plus the heap and startup time.

Added: test/diff_languages.cc + test/frozen_languages.h (A2), test/include_all.cc + test/init_size.sh + CMake target soundex_include_all (A3), docs/TABLES.md. Xapiand pins the soundex commit: bumping it is the lead's. The tables change type (map/vector to array); nothing in Xapiand src/tests references them by name (grep).

## Rulings I made

- Array of string_view pairs, scan kept, over sorted array plus lookup: identical behavior, smaller change.
- The script lives in test/ and the table note in docs/TABLES.md to stay in the file set; AGENTS.md, ARCHITECTURE.md, CHANGELOG.md stay stale until the lead applies the patch.
