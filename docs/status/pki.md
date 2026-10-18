# pki
- Design: docs/design/pki.md at d05ed6bab (revision 1); reviews:
  docs/reviews/pki-design.md (architect, preliminary: REJECT, 2026-10-17, a37a511ae),
  docs/reviews/pki-security.md (security: APPROVE WITH CONDITIONS, 2026-10-17, e67ec4875)
- Branch / worktree: design/pki at /Users/kronuz/code/Kronuz/Xapiand-wt/pki
- State: implementing. Step 1 is in `Kronuz/pki` (public, created 2026-10-18) at 0ee2841,
  and D5's identity code at 4e7d279; CI green on all 11 jobs at both
- Library: `Kronuz/pki` at /Users/kronuz/code/Kronuz/pki, branch main (the new repository's
  own main, per Q1); its CI is `.github/workflows/ci.yml` over cpp-libraries' matrix
- Done criteria (set by the lead; contributors tick, never edit): K1 to K8 below, for steps
  1 to 3; none is whole yet, and "Step 1's evidence" says which rows' parts pass
- Done: ledger started (ad387521c); revision 0 drafted (58aa0c7ee) and marked ready
  (f855f3e42); both preliminary reviews read with their probes (.scratch/review-pki,
  .scratch/review-pki-security); revision 1 written, folding every finding and the owner's
  SYS-2 and SYS-13 (d05ed6bab); the re-checks read (af23d6bc0 architect, bfc11c50c
  security). In `Kronuz/pki`:
  - 57cdb21: step 1. The pki::pki library and pki::run (D1), kronuz-pki as its driver;
    D3's checks (the private-file header with three policies, SYS-15; the authority
    directory; the filesystem allowlist; placement; passphrase files), D3's secrets, D4's
    keys and scrypt PKCS#8 through OSSL_STORE, D10's `fingerprint`, D11's `check`; CI.
  - 0ee2841: the first CI run's two failures fixed. FreeBSD and macOS give a new file its
    directory's group, so the test's own-group cases set the group (pki's refusal was
    right); io is fetched whenever its target is missing; refusals quote what they repeat.
  - 4e7d279: D5's identities (Q7): SPIFFE IDs, the trust domain and name rules, the cluster
    refusal (PKI-R25), T13's vectors.
- Doing: nothing; step 1 and D5's identities are done to CI green
- Next:
  1. The lead decides whether a code and security review of step 1 runs now or with steps 2
     and 3 (K8 asks for one over the implementation).
  2. Step 2 (D3's index and commit, D5's profile and signing, D6, T14's fuzz targets) waits
     for tls/d1-gate to land, as the task says; its parts that touch no Xapiand file (the
     index, the profile, signing, T1, T3, T4, T8, T13 in the library) could start earlier on
     the lead's word.
  3. Design-text conditions for the design's owner (this contributor implements; it does not
     edit decisions): C13 (PKI-R20: state D1's io linkage; pki now has `PKI_LINK_IO`, OFF
     for a consumer that compiles io.cc, and step 2's build checks one copy), C14 (PKI-R21:
     `io::full_fsync` on the temp index; pki's `Directory::create` already syncs with it),
     C16 (PKI-R23: close format 1's event kinds), C17 (PKI-R24), C19 (PKI-R26), C12
     (PKI-R19: renewal from a Kubernetes Secret); security C9 (SR15: `recertify
     --expect-spki`), C11 (SR16: `passphrase` removes the old file), C12 (SR17: Q8's
     reading). SR14's code is done (below); its `backup` notice is step 3's.
  4. The lead's items from before, unchanged: Detent's items through Detent's agent (PKI-R12,
     flags 3 and 4, Q3), and the TLS gate's reading of "the service's own group" as the
     effective group (D3), which pki implements as `getegid()`.
  5. When the library lands: its entry in cpp-libraries' index.
- Blocked on: nothing for step 1. Step 2 on tls/d1-gate landing (the task's sequencing);
  Q3 is Detent's maintainer's
- Findings folded in: in d05ed6bab, PKI-R1 to PKI-R18, SR1 to SR13, SYS-2, SYS-13, SYS-15.
  In `Kronuz/pki`: SR14 (CA keys and passphrase files at 0600 only, 57cdb21), PKI-R22 (the
  Linux allowlist, the refusal naming the type, 57cdb21), PKI-R25 (the cluster rule,
  4e7d279), SR17's attribution (4e7d279); PKI-R20's seam (57cdb21)
- Last verified: 2026-10-18. Built and tested on this Mac (`quiet check` exit 0) against
  Homebrew OpenSSL 3.6.5 and 4.0.3, and under ASan and UBSan with Homebrew LLVM: all 7
  tests pass at 4e7d279's tree. CI run 39092514425 at 0ee2841: 11 of 11 jobs green
  (Linux x86_64 GCC and Clang and arm64 on OpenSSL 3.0.13; macOS arm64 on 3.6.4 and x86_64
  on 3.6.3; macOS arm64 on 4.0.2; FreeBSD 14.3 on base 3.0.16; ASan+UBSan and TSan on
  Linux; the owner cases as root on Linux, 109 checks, and macOS, 110). CI run 39092757204
  at 4e7d279: 11 of 11 green

## Owner decisions on revision 1's open questions (2026-10-17)

Made by the lead under the owner's standing permission for educated calls, each the
recommendation both reviews share unless noted:

- Q1: a new public `Kronuz/pki` repository, created when step 1's implementation starts.
- Q2: `xapiand pki` as the command; `kronuz-pki` builds only in the library's CI, never packaged.
- Q3: Detent's maintainer decides; proposed in `Detent/docs/SHARED_LIBRARY_DECISIONS.md`.
- Q4: no revocation in step 1.
- Q5: 365-day leaves with a `--max-days` cap and the exposure stated in days (the architect's
  reasoning: renewal keeps the key, so a shorter life does not bound an undetected theft).
- Q6: enrollment deferred to the peer transport design (`strata` branch `design/peer-transport`).
- Q7: SPIFFE IDs.
- Q8: reactor's CN fallback becomes an opt-in flag, off by default (additive; shared with Detent).
- Q9: the shared file check lives in `io`; not blocking.

## Done criteria for step 1 (set by the lead, C11; tick with evidence, never edit)

- [ ] K1 T1, T4, T5, T10, T13 pass in the library's own suite (profile, refusals, clocks,
  fingerprints, identity vectors).
- [ ] K2 T2 and T3 pass: transport-security's T1 and T2 on pki certificates with the same
  assertion counts, and reactor's `Peer` handshakes accept and refuse as T3 states.
- [ ] K3 T6 and T7 pass on a 3-node HTTPS cluster (planned and emergency rotation), and T11 and
  T12 (backup, restore, lost authority).
- [ ] K4 T8 passes: every kill point leaves the directory whole, under umask 000 and 077.
- [ ] K5 T9: CI green on Linux x86_64 and arm64, macOS, and FreeBSD, plus ASan+UBSan and TSan.
- [ ] K6 T14: one CPU-hour of fuzzing per intake under a host hold, zero findings.
- [ ] K7 Conditions PKI-R19 to R26 closed at file:line, and every condition of the security
  re-check (`docs/reviews/pki-security.md`) closed.
- [ ] K8 Code review and security review of the implementation (security tier) approve.

## Step 1's evidence (toward the criteria above; none ticked)

What passes in `Kronuz/pki`'s suite, by row; the rest of each row needs pki's own signing
(step 2) or Xapiand (step 4).

- T4's file cases (`test/files_test.cc`): a CA key or passphrase file at 0640 refused even
  with the caller's group (SR14), at 0644 and 0660 refused, at 0600 and 0400 accepted; a node
  key at 0640 and 0440 accepted only with the caller's effective group, refused with another
  group, and at 0644, 0660, and 04600; a symlink, two links, a FIFO, and a directory
  refused; a file and a directory owned by `nobody` refused, run as root in CI;
  `DIR` at 0750, 0770, and 0500, a symlink, and a file refused; a passphrase file inside
  `DIR`, in a subdirectory, and through a symlink refused; a path under `.xapiand/node`
  refused. Missing: the request intake, which is step 2.
- T8's mode cases: under umask 000, 077, and 777, `make_directory` gives 0700 and
  `create` gives 0644 and 0600, refusing an existing name and a planted symlink; the CA key
  is PKCS#8 with PBES2, scrypt N = 16384, r = 8, p = 1, a 16-byte salt, and AES-256-CBC
  (parsed from the PEM), read back by pki through OSSL_STORE and by `openssl pkcs8` on
  every CI job (3.0.13, 3.0.16, 3.6.3, 3.6.4, 4.0.2) with 28- and 1,023-byte passphrases; a
  wrong passphrase refused by both; under 12 characters refused. Missing: the kill points,
  the core-file case, and the index's refusals, which are step 2's.
- T5's verification cases (`test/expiry_test.cc`, on a fixture leaf with D5's dates):
  verified at issuance less 3,599 s and notAfter less 1 s, refused at less 3,601 s and
  notAfter plus 1 s, by `X509_verify_cert` and `openssl verify -attime`; `check` exits 1
  past five sixths of the life and within `--within`, 0 before, with `PKI_TEST_NOW`.
  Missing: pki-signed leaves, `list --expiring`, and the clock refusal (step 2).
- T10 (`test/fingerprint_test.cc`): the SPKI digest equals the CLI's `x509 -pubkey | pkey
  -outform DER | dgst -sha256` for P-256, P-384, and RSA leaves, and differs from
  `X509_pubkey_digest`'s; the DER digest equals the CLI's; both are 64 lowercase hex
  digits; a same-key leaf keeps the SPKI digest and changes the DER digest. Missing:
  consensus's `certificate_fingerprint` (with T3, step 2) and `request`, `show`, and `sign`
  printing it.
- T13 (`test/identity_test.cc`, 4e7d279): every vector the row lists, plus the cluster rule
  (PKI-R25).
- T9: the matrix above. Missing: TSan over code with threads, which step 1 has none of.

