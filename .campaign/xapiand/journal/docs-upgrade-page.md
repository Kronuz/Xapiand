# docs-upgrade-page: journal

State: waiting on jobs 261109-152541-04dbf7
Quest: .campaign/xapiand/quests/docs-upgrade-page.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Page written and committed (`docs/src/content/docs/upgrade.md`). The `e2e` check is queued
(261109-152541-04dbf7, light, 120 min: cmake build plus `run_all.sh smoke e2e tls`). A2 not started.

## Next

1. When the e2e job passes at the head, fill A4's row (and A3's, with the reviewer's read).
2. A2: needs an alpha.10-line (Xapian) binary, which does not exist on this machine (the root `xapiand` is an
   x86 Mach-O that does not run on this arm64 host). Build `origin/xapiand` (Xapiand:e692a64b1) and the
   head in one job, index documents with the old binary, `--dump`, `--restore` with the new one, compare. Needs the lead's go-ahead (two full builds at load 84).
3. When `sortable-encoding` lands, re-read the "Stored documents and numbers" first bullet against its changelog line.

## Blocked on

1. A2: lead decision. Recommendation: after `xapiand-final-release` produces a Xapian binary, run the two-binary
   round trip once as a light job (about 2 builds); until then A2 has no evidence.
2. `sortable-encoding` is not landed (blocked on the owner's Q1-Q3). The numbers bullet follows its design
   D1/D4/D5 (exact cross-type order; NaN, uint64 >= 2^63 answer 400; -0.0 is 0) and must be confirmed against the owner's answers.
3. For the lead (outside my file set): the refusal message in `src/main.cc:663` and the changelog entry say
   "alpha.9 or earlier", but alpha.10 data is Xapian too (audit gap 4); the binary links
   `/upgrade/#upgrading-from-100-alpha9`, which the page keeps with an explicit anchor.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/src/content/docs/upgrade.md` |

## Result

<When ready for review.>

## Rulings I made

1. File name: kept `upgrade.md` (not `upgrading.md`): the sidebar (`docs/src/lib/sidebar.mjs:420`), the changelog and
   the Xapian-data refusal message (`src/xapian_data_guard.h:104`) link `/upgrade`. A1's path is therefore `upgrade.md`.
2. Kept the 0.40.0 content (ids, rolling back) in the same page; the alpha.9/alpha.10 section leads.
3. Left out the audit's nested-index refusal (#21): the changelog now says indexes nest (the owner reversed it).
4. Left out limits for the not-yet-landed `--search-walk-queue` rows and #36-37 details beyond shutdown bound.
