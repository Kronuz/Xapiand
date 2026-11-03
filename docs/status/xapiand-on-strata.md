# xapiand-on-strata

- Design: docs/design/xapiand-on-strata.md (revision 1); reviews: none yet.
- Branch / worktree: design/xapiand-on-strata from main 6e13416, at /Users/kronuz/code/Kronuz/strata-wt/xapiand-on-strata
- State: Phase 7a gate passed (2026-10-29); the owner split Phase 7's gate the same day (single-node 7a and 7c, then Phase 8; 7b later, PLAN.md). Next: the 7c design on Xapiand:addacf386 + Xapiand:0d194335b: the architect's gate review (docs/reviews/phase-7a-gate.md) rejected on L1 and L2, and its re-check (strata:5e34482) closed L1, L2 and C1-C4; C5 (Linux, FreeBSD, arm64) is owed before gate 7b. The rest of this header records the design stage.
- Done criteria (set by the lead; contributors tick, never edit): none set yet.
- Done:
  - The seam inventory (call-site counts by group, from Xapiand 855f75281), the coverage and gap table, decisions D1 to D12, the ordered path (X1 to X5, P1 to P4, L1, S1 to S5), gate rows T1 to T10, and open questions Q1 to Q3.
- Doing: nothing. The design stops at the first buildable steps (X1, X2; P1 after Q2; L1's design).
- Next: the architect's preliminary review; the owner's Q1 to Q3; the lead writes done criteria and launches X1 and X2.
- Blocked on: Q2 (opaque `bytes` slots for 7a) blocks P1; Q3 (queryparser on the 7a path) blocks S4. Nothing blocks X1 to X4.
- Findings folded in: none yet.
- Last verified: nothing compiled or run (designer, read-only study). Counts come from grep over `git ls-files src` minus `src/xapian/`; line citations checked against the files at the commits named in the design header. One disclosure: the first 140 lines of `src/server/remote_protocol_views.cc` were read before checking PLAN lists it as GPL; nothing past them.

## Clean-room note (2026-10-20)

While drafting, the designer read lines 1-140 of Xapiand's `src/server/remote_protocol_views.cc`
(includes and the constructor) before noticing that PLAN lists it as GPL. Nothing past line 140 was
read, and the design takes no content from it: its remote and replication rows rest on grep counts
alone. That file is replaced by the shard RPC in 7b and is never ported. `src/xapian/` and
`sortable_serialise` were never opened.

## Owner answers (2026-10-20)

Q1 single-node 7a, Q2 opaque `bytes` slots first, Q3 `queryparser` on the 7a path. Next: X1, P1, L1.

## Owner answers (2026-10-29)

- Phase 7's gate splits: single-node (7a and 7c), then Phase 8; 7b later (PLAN.md).
- Non-Apple storage keeps the ordering barrier, with a warning, rather than a full flush by default.
- The queryparser accepts wildcards, fuzzy and partial words inside phrases (match_phrase_prefix), after 7c.
- Removed features stay removed when nobody would really use them or a native alternative is
  coming; the owner hears about the rest. Two have real users and native replacements, proposed
  for after 7c: "more like this" (re-analyze the seed documents' stored text, as Elasticsearch
  does without term vectors), replacing `?fuzzy` and `?nearest`; and `min_score` in the
  collector, replacing the weight cutoffs.

## Owner answers on the 7c design (2026-10-29)

- Q1: delete Xapiand's Xapian path soon, at 7c's start; the closing benchmark compares against the
  final Xapian-only release.
- Q2 (lead's call, delegated): Xapiand's WAL stays as it is until 7b.
- Q3: remove the offload path from the http library entirely (worker pool, `should_offload`, the body hand-off to workers). Callers migrate to coroutine handlers: Xapiand, prism (`src/main.cc:317`, `:325`, `workers=2`) and Detent (coordinated with its agent; none running on 2026-10-29).
- Q4: not decided. The `stored` type was meant for very large files, with a Haystack-style blob
  engine in mind. Whether anything is kept needs its own architecture weighing and experiments;
  7c only makes blob reads and writes non-blocking and does not move blobs into strata.
- Q5: the twice-RAM page-fault row gates at Phase 6.
- Q6 (lead's call, delegated): read-your-writes stays the search default in 7c.
- Clustering (owner, 2026-10-29): deleting Xapian removes the remote and replication services,
  so the single-node release has no cluster until 7b lands. Accepted.
- Before release (lead, 2026-10-30, not a priority): emit the non-Apple barrier warning (completion
  `PosixIO`) and wire `full_flush_ordering` through strata and a Xapiand switch. Only macOS on
  external or third-party drives is affected; Linux and FreeBSD are unchanged.
- Follow-up (lead, 2026-10-30): prospect's sliced search (prospect:c77d6eb) costs multi-term queries
  that fit in one slice 1.4-2.2% against the unsliced loop; the residual's cause is unfound (not the
  checkpoint, not code placement). A performance engineer takes it before the 7c gate's T12.
- Dates (owner, 2026-11-02): strict ISO 8601, as datetime's head parses it. An empty fraction
  (`10:00:00.Z`), a fraction over 62 digits and a negative hour, minute or second are not dates;
  schema detection types them as text.
- The 2026-11 pin wave's other result changes are accepted: malformed boolean queries answer 400;
  the similarity sort follows the corrected metrics; a bare LF closes the connection
  (`lenient_http_headers` opts out); `normalize_path` keeps `os.path.normpath` (`""` is `.`), and
  Xapiand refuses an empty data directory or an index path that normalizes onto or outside its base.
