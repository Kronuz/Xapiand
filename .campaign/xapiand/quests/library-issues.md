# library-issues: The library performance issues the cpp-libraries gate filed

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status library-issues`.

## Outcome

Every issue the cpp-libraries gate of 2026-11-01 filed under the owner's account is settled: a
verified win lands in its library with CI, a loss or a justified trade is written on the issue and
the issue closed with the reason, or it stays open with what would settle it. The queue is
cpp-libraries `docs/status/library-issues-2026-11.md` (moved from strata at strata:557d97a0).
Repository: Kronuz/cpp-libraries (each fix in its own library).

## Acceptance criteria

Settled already (the ledger, 2026-11-02): stash#1 (landed stash:d2e8e59, history quest
`stash-claim-hint-publish`), threadpool#3 and strict-stox#2 (closed, measurement artifacts),
threadpool#2 (moot: `ThreadPool` removed), docset#1 (closed: code placement; `explore/bitmap-and`
docset:56f7b55 keeps the A/B).

- A1 [artifact] termdict#1, point lookup: its share of strata's query and upsert profiles measured before any format-changing skip table (ledger: explored, open; three experiments lost in termdict `docs/perf/point-lookup.md`, `explore/point-lookup` termdict:c95bb4f): `docs/status/library-issues-2026-11.md`
- A2 [manual] logger#1: the i9's back-to-back row (83.6 ns against fmt + mutex's 62, logger `docs/status/producer-rings.md`) closed, or the issue left open with the numbers: the lead names the issue state
- A3 [manual] journal#1 (a durable append makes three full flushes against one): settled with 7b, where the journal replaces the WAL (owner, 2026-11-02): the lead names the issue state
- A4 [manual] allocators#1 (one-block pool pair): started and stopped; settled or closed with the reason: the lead names the issue state
- A5 [manual] Wave 1's queued regressions (strings#2, times#1, fs#1, enum-reflection#1) and waves 2 and 3 (the ledger's lists): each reproduced against an A/A and fixed, or closed with the reason, after the release: the lead names each issue's state

## Contracts

None: each fix keeps its library's API unless its own quest says otherwise.

## Decisions

- Owner, 2026-11-02: take only the valuable ones for now; the rest after the release. The queue was the
  Xapiand lead's (Detent's lead was told so, 2026-11-02).
- The gate itself (cpp-libraries `docs/reviews/gate-2026-11-01.md` and its per-repo files) and the October
  2026 surveys and audits were run outside both campaigns; this quest carries only their open issues.
- Blocked (owner): after the release, except A1 if strata's profile shows point lookup on the hot path.

## May land early

Each issue's fix, landed in its own library with CI, any time.
