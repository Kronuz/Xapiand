# pki
- Design: docs/design/pki.md at 58aa0c7ee (revision 0); reviews: none yet
- Branch / worktree: design/pki at /Users/kronuz/code/Kronuz/Xapiand-wt/pki
- State: designing (ready for preliminary review)
- Done: ledger started (ad387521c); sources read (transport-security revision 3
  and its review, src/tls.{h,cc}, reactor_tls.h at e19afd9, consensus
  transport/tls.h at fc66c0e, Detent's TLS designs and maintainer proposals,
  strata track-c, the Kronuz catalog); openssl probe of a two-CA bundle and of
  DER against SPKI pins in .scratch/pki-probe; revision 0 drafted with D1 to D12,
  T1 to T10, Q1 to Q6 (58aa0c7ee)
- Doing: waiting for the architect's preliminary review of revision 0
- Next: 1. Answer the review's findings in revision 1 (a response section, as in
  transport-security). 2. The owner answers Q1 to Q6; Q3 goes to Detent's
  maintainer. 3. No implementation until both land.
- Blocked on: the architect's review; the owner's answers (Q1 repository, Q2
  command, Q4 revocation, Q5 leaf life, Q6 enrollment); Detent's maintainer (Q3)
- Last verified: 2026-10-17
