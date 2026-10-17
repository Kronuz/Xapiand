# pki
- Design: docs/design/pki.md at d05ed6bab (revision 1); reviews:
  docs/reviews/pki-design.md (architect, preliminary: REJECT, 2026-10-17, a37a511ae),
  docs/reviews/pki-security.md (security: APPROVE WITH CONDITIONS, 2026-10-17, e67ec4875)
- Branch / worktree: design/pki at /Users/kronuz/code/Kronuz/Xapiand-wt/pki
- State: designing (ready for preliminary review)
- Done criteria (set by the lead; contributors tick, never edit):
  - none yet; the lead writes them before step 1 (the design's Steps and PKI-R18 name the
    rows)
- Done: ledger started (ad387521c); revision 0 drafted (58aa0c7ee) and marked ready
  (f855f3e42); both preliminary reviews read with their probes (.scratch/review-pki,
  .scratch/review-pki-security); revision 1 written, folding every finding and the owner's
  SYS-2 and SYS-13 (d05ed6bab)
- Doing: waiting for the preliminary re-checks of revision 1 (architect, security reviewer)
- Next:
  1. The re-checks of revision 1.
  2. The owner answers Q1, Q2, and Q4 to Q9; Q3 goes to Detent's maintainer as SYS-2's
     proposal.
  3. The lead sends Detent's items through Detent's agent: its TLS key files are unchecked
     (PKI-R12), and SYS-13's own-group rule is stricter than its relay's (flag 4); the
     reactor::pki row at DEPENDENCY_UPGRADES.md:79 is this library (flag 3); SYS-2's SPKI
     pins, with D7's restart order until then (Q3).
  4. The lead confirms with the TLS gate that "the service's own group" means the effective
     group (D3), so both checks read SYS-13 alike.
  5. The lead writes the done criteria. Step 1 (D1, D4, D10, D11, D3's checks) starts once Q1
     is answered; D5's identity code waits on Q7; step 2 starts after tls/d1-gate lands.
- Blocked on: the re-checks (architect, security reviewer); the owner: Q1 repository, Q2
  command, Q4 revocation, Q5 leaf life, Q6 enrollment, Q7 identity form, Q8 reactor's CN
  fallback, Q9 the private-file check's home; Detent's maintainer: Q3 (SPKI pins, SYS-2)
- Findings folded in, all in d05ed6bab: PKI-R1, PKI-R2, PKI-R3, PKI-R4, PKI-R5, PKI-R6,
  PKI-R7, PKI-R8, PKI-R9, PKI-R10, PKI-R11, PKI-R12, PKI-R13, PKI-R14, PKI-R15, PKI-R16,
  PKI-R17, PKI-R18; SR1, SR2, SR3, SR4, SR5, SR6, SR7, SR8, SR9, SR10, SR11, SR12, SR13; the
  owner's SYS-2 and SYS-13; the system review's SYS-15
- Last verified: 2026-10-17. Every file:line revision 1 cites re-read at the commits it names
  (Xapiand a37a511ae, reactor e19afd9, consensus ef30b4f, completion 5251cf9, io 1336f85,
  char-classify 306ecd9, strings b42d3c4, fantasyname a87942a, strata 5660f82, Detent
  488522d); SPIFFE's SPIFFE-ID.md and X509-SVID.md, RFC 9525, and Dogtag's packaging read on
  the web; `gh repo list Kronuz --limit 1000` (257 repositories). Nothing built or probed:
  the host is under a strict hold (strata-trials), and the design cites the reviews' probes.
