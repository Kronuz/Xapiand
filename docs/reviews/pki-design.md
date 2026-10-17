# Xapiand review: PKI helper design

- Date: 2026-10-17.
- Reviewed: `docs/design/pki.md` revision 0 at 58aa0c7ee (348 lines) and its ledger
  `docs/status/pki.md` at f855f3e42, on `design/pki` in `Xapiand-wt/pki`. Xapiand source and
  harness at f855f3e42, identical to master's 7e8cccb7d outside `docs/`. reactor e19afd9
  (Xapiand's pin, `CMakeLists.txt:797`); consensus fc66c0e; completion 5251cf9; cluster
  84e6b01; Detent 68ab6c6 and strata 3c51ee5, read only.
- Checked against: transport-security revision 3 and my review of it, whose rulings bound this
  design: TS-R2 (the profile), TS-R5 (emergency rotation), TS-R7 (request policy), TS-R8
  (escrow and recovery), TS-R9 (roles in the SAN), TS-R17 (issuance outside reactor);
  `KRONUZ_REFACTORING_DIRECTIVES.md` §1, §6, §9; Detent's `AUTHORITY_TRANSPORT.md`,
  `PERSISTENT_TRANSPORT_ARCHITECTURE.md`, `LIBRARY_BOUNDARIES.md`, `DEPENDENCY_UPGRADES.md`,
  `RELAY.md`, `USAGE.md`, `ASYNC_AUTHORITY_ARCHITECTURE.md`, and its authority source where
  the design describes its behavior; strata's `docs/design/track-c.md` and `docs/PLAN.md`.
  The security review, `docs/reviews/pki-security.md` at e67ec4875 (SR1 to SR13), was read
  after this review's findings were written; "The security review" compares the two.
- Checked by: reading every cited path; re-deriving the design's own probe
  (`.scratch/pki-probe`); and probes in `.scratch/review-pki/probes/`, each with its `.out`,
  run against Homebrew OpenSSL 3.6.5 and 4.0.3, macOS LibreSSL 3.3.6 and curl 8.7.1, and
  Python 3.15: `profile_check.sh` (D5 under T1's checks), `cn_fallback_tls.sh` (the CN as a
  hostname, over real handshakes), `serial_length.py`, `quorum_model.py` and
  `trace_quorum.py` (Detent's voters through a renewal and a rotation),
  `same_subject_bundle.sh`, `attime_bounds.sh`, `pkcs8_scrypt.out`, `crl_expiry.sh`,
  `rsa_ku_static.sh`, `spki_digest.out`, `tls_certs_profile.out`, `index_scale.py`,
  `cn_length.out`, `empty_subject.sh`. `quiet check` returned 0 before the probes and 1 on
  resuming (the strata gate VM run's strict hold); nothing was compiled.
  - Not run: listed at the end.
- Citations: `pki:N` is line N of `docs/design/pki.md`, `ts:N` of
  `docs/design/transport-security.md`, `review:N` of `docs/reviews/transport-security-design.md`.
  `reactor/`, `consensus/`, `completion/`, `cluster/`, `strings/`, `char-classify/`, and
  `strata/` are siblings under `~/code/Kronuz/`; `Detent/` is `~/code/Detent/`; `probes/` is
  `.scratch/review-pki/probes/`.

## Verdict

**REJECT: the profile (D5) and the authority's records (D3, D6) do not hold as written, the
documented Detent rotation loses quorum in 4 of 6 restart orders, and a lost authority has no
procedure.**

The form (D1, D2), the key types (D4), the fingerprints (D10), and the expiry checks (D11)
hold. Nothing in revision 0 changes reactor, consensus, completion, or Detent today (pki:68).
The profile and the authority directory are formats: leaves issued under D5 live 365 days and
the authority 1,826 days (pki:189-190), so their defects are cheaper to fix on paper than
after the first `pki init`. Work that touches neither (the library's skeleton and CI, key
generation, fingerprints, expiry checks) can start; the profile, the index, and the rotation
commands wait for revision 1. The security review approves with conditions on revision 1,
before step 1's code, so both reviews hold the same work; "The security review" gives where
they differ.

What holds, and reproduces:

- The design's probe reproduces (`.scratch/pki-probe`, OpenSSL 4.0.3): one key under two CAs
  gives DER digests 5aa4a341… and 3a3769d5… and one SPKI digest 635f6ca8…, and
  `openssl verify -CAfile trust.pem` accepts both leaves.
- D5's profile passes T1's own checks on 3.6.5 and 4.0.3: each role's leaf verifies under
  `-x509_strict` with its purposes, and a server leaf presented as a client and a client leaf
  presented as a server are refused with error 26 (`probes/profile_check.out`).
- reactor is cited correctly: TLS 1.2 and later, no renegotiation, no early data, and a 64 KiB
  certificate-list cap for every role (`reactor/reactor_tls.h:64-69`); Server verifies
  nothing, Peer requires both ends, Client verifies the server (`:83-85`); one CA file through
  `load_verify_file` (`:81`). Neither reactor nor http loads a CRL, checks a URI SAN, or swaps
  a context (no match in either tree).
- consensus is cited correctly: a SHA-256 of the leaf's DER
  (`consensus/include/consensus/transport/tls.h:12-20`), one unique pin per voter with the
  local leaf matching its own (`:21-36`), and a constant-time scan of every pin (`:47-66`).
  Pins are transport options, outside `FixedConfiguration` and the `DTPROF01` profile
  (`transport/profile.h:10-37`, `transport/peers.h:27-38`, `consensus/types.h:43-48`), so a
  pin change is a configuration edit and a restart (pki:50).
- Xapiand's key check (`src/tls.cc:30-32, 46-53`), its data-root marker
  (`src/manager.cc:292, 309`), its ASCII lowercasing of node names (`src/node.h:139-142`),
  `harness/tls_certs.sh`'s leaf with only a SAN and an EKU (`probes/tls_certs_profile.out`),
  and Detent's RSA fixture (`Detent/tests/detent_authority/tls_certificates.cmake:12-23`) are
  as stated.
- D1 follows TS-R17 and the directives' library mechanics (§9). D2 keeps TS-R6's and TS-R7's
  attack surface out of step 1. D5 and D6 take TS-R7's request policy (authority-set
  extensions, a 16 KiB cap, a backdated notBefore) and TS-R2's exact, wildcard-free identity.
  D8 answers TS-R5.
- No Kronuz library signs certificates; only tests do (`reactor/test/tls_test.cc:42-74`,
  `http/test/tls_asio_test.cc`, `Xapiand-wt/tls-gate/tests/test_tls.cc`). OpenSSL 4.0.3's
  headers keep every call the library needs (`X509V3_EXT_conf_nid`, `X509_REQ_verify`,
  `EVP_PKEY_Q_keygen`, `PEM_write_bio_PKCS8PrivateKey`, `PKCS5_pbe2_set_scrypt`).

What blocks: PKI-R1, PKI-R2, and PKI-R4 to PKI-R8, one line each under "Required changes".
The rest are conditions.

## Rulings

| Decision | Ruling | Findings |
|---|---|---|
| D1 Library and command, own repository | Sound. The command's entry point belongs in the library target, so every consumer runs the same code (§9 builds examples only at top level) | PKI-R3, PKI-R18 |
| D2 Offline only in step 1 | Sound. TS-R8's escrow and recovery land here, not with enrollment | PKI-R7 |
| D3 The authority directory | Sound with changes: no commit across files, no version, no directory check, two wrong claims | PKI-R4, PKI-R6, PKI-R12 |
| D4 Keys and signatures | Sound with changes: say why RSA requests are accepted | PKI-R11 |
| D5 The profile | Sound with changes: the URI lacks the role and a canonical form, and the CN is a hostname to OpenSSL and curl | PKI-R1, PKI-R5, PKI-R11 |
| D6 Issuance and uniqueness | Sound with changes: the request picks the identity, and every renewal needs `--replace` | PKI-R4, PKI-R8 |
| D7 Planned rotation | Sound with changes: the overlap holds; the procedure misreads Xapiand's step 1, misses Detent's restart order, and has no abort | PKI-R2, PKI-R4, PKI-R9 |
| D8 Emergency rotation | Sound with changes: it needs public keys the authority does not keep | PKI-R4, PKI-R7 |
| D9 No revocation in step 1 | Sound; Q4's argument is wrong | PKI-R13 |
| D10 Fingerprints | Sound. T10 pins the SPKI computation | PKI-R16 |
| D11 Expiry visibility | Sound | — |
| D12 The harness adopts it | Sound with changes: say how T2's dated leaves are made | PKI-R16 |

| Row | Ruling | Findings |
|---|---|---|
| T1 Profile | Target right; the serial assertion fails on 1 issuance in 256 | PKI-R16 |
| T2 Xapiand handshakes | Rerunnable (`harness/e2e_capture.sh:26-53`) | PKI-R16 |
| T3 Peer handshakes | Rerunnable in reactor's memory-BIO pattern (`reactor/test/tls_test.cc:76-108`) | — |
| T4 Issuance refusals | The owner case needs root; the concurrency case is ambiguous | PKI-R16 |
| T5 Expiry and clocks | Rerunnable; OpenSSL 3.6.5 and 4.0.3 disagree at notAfter | PKI-R10, PKI-R16 |
| T6 Planned rotation | Cannot run as written: `cluster_check.sh` speaks plain HTTP | PKI-R9, PKI-R16 |
| T7 Emergency rotation | No accept case | PKI-R16 |
| T8 Files | "Every file old or new" passes with a broken directory | PKI-R6 |
| T9 Platforms | Name each job's OpenSSL, and TSan's scope | PKI-R16 |
| T10 Fingerprints | Rerunnable; it catches the likely bug | PKI-R16 |

The open questions Q1 to Q6 have my recommendations under "Open questions".

## Findings

### Interface-affecting

These are the findings where the design touches, or asserts, what reactor, consensus,
completion, or Detent will do.

#### PKI-R1. The identity URI omits the role, has no canonical form, and has no consumer (blocking)

Evidence:

- TS-R9 requires roles in the SAN, `.../node/...` against `.../client/...` (review:381-382).
  D5 gives every role `<scheme>://<cluster>/node/<name>` (pki:193). A `client` leaf verifies
  with `-purpose sslclient` (`probes/profile_check.out`), which is what a Peer acceptor
  checks, and its URI names a node. D6's index then treats client `n1` and node `n1` as one
  identity.
- "Lowercased and percent-encoded outside RFC 3986's unreserved set", compared byte for byte
  (pki:193), leaves the bytes undefined:
  - ASCII or Unicode lowercasing. Xapiand's is ASCII only (`src/node.h:141`, through
    `strings/strings.hh:336-339` to the table at `char-classify/chars.hh:176-212`).
  - The hex case of `%XX`. RFC 3986 §2.1 says producers "should" use uppercase; two
    implementations that differ fail the comparison.
  - The cluster's case. Xapiand compares cluster names byte for byte (`cluster/bus.h:111`)
    and defaults to `Xapiand` (`config.h.in:281`); lowercasing merges `Prod` and `prod`,
    which discovery keeps apart.
  - Empty names, names holding `/`, and a maximum length.
- pki:197-199 says the URI is "read by the reactor check strata adds". Strata's track-c
  authenticates a peer "against its pin in the node directory", and pins "rotate there
  without a `Configuration` entry", applied without a restart
  (`strata/docs/design/track-c.md:212-215, 766-771`). No strata design adds a URI check.
  transport-security's answer "Leaf pins: no" (ts:153-154) predates that directory. Two
  approved designs disagree on what identifies a node under strata, and this profile is where
  the disagreement lands.

Why it matters: the URI is the interface a future `reactor::tls` check (TS-R17), Detent's
adoption (Q3 option 4), and bring-your-own certificates (review:159) would read. A form fixed
after leaves exist needs a window in which every verifier accepts both forms.

Required change:

1. Put the role in the path: `/node/` for `server` and `peer`, `/client/` for `client`. Key
   D6's uniqueness on role and name.
2. Specify the canonical form as an algorithm with test vectors: ASCII-only lowercasing as
   `strings::lower` does; every byte outside the unreserved set as `%XX` in uppercase hex;
   empty names refused; a stated length cap. With the cluster fixed at `pki init` (PKI-R8),
   case merging cannot happen inside one authority.
3. Or use SPIFFE IDs (`spiffe://<trust domain>/<consumer>/<role>/<name>`). SPIFFE forbids
   percent-encoding, which removes item 2's questions, and SPIFFE-aware proxies and
   SPIRE-issued bring-your-own leaves interoperate. The cost: names outside `[A-Za-z0-9._-]`
   are refused, a constraint on `--name` that Xapiand does not impose today. The owner
   chooses.
4. State that the URI is advisory until a design adopts a check, and put the strata question
   to the owner: does a strata peer verify a pin from the node directory or a URI under the
   cluster CA? The profile can carry both; D6's rules differ (PKI-R2, PKI-R8).

#### PKI-R2. Detent's documented rotation loses quorum in most restart orders, and Q3's options change consensus (blocking)

Evidence:

- pki:121-123 and Q3 option 1 (pki:314-316): "with 3 voters a rolling change keeps quorum
  (2 of 3)". `probes/quorum_model.py` models one pin per voter (`tls.h:24-31`) and
  `authenticated_voter`'s scan (`:47-66`): a link is up when each end accepts the leaf the
  other presents, and quorum needs a voter linked to a majority, itself included. Renewing one
  of 3 voters keeps quorum in 2 of 6 restart orders: a peer, then the renewed voter, then the
  other peer. The other 4 lose quorum during the second restart. With 5 voters all 120 orders
  keep it (`quorum_model.out`). Detent allows 3 voters
  (`Detent/src/detent_authority/service.cc:308-310`).
- D7's phase 2 renews every leaf. With DER pins, 3 voters need at least 5 restarts, in a
  schedule that pins a voter's new leaf before the voter presents it (one such: 0, 2, 1, 0,
  2; `trace_quorum.out`). With SPKI pins and same-key renewals, 3 restarts in any order keep
  every link up.
- "Removing a voter's pin refuses that voter" (pki:51): `validate_peer_identity` throws unless
  every configured voter has exactly one pin (`tls.h:24-27`), so a pin cannot be removed while
  its voter stays. Removing the voter is a membership change that durable recovery rejects
  (`Detent/docs/AUTHORITY_TRANSPORT.md:49`). Detent revokes a leaf by replacing its pin, which
  is re-keying the voter.
- Q3's options are interface changes. Option 2 relaxes one pin per voter (`tls.h:24-27`).
  Option 3 adds an SPKI digest beside `certificate_fingerprint` (`tls.h:12-20`) and changes
  Detent's `certificate_sha256` field (`Detent/src/detent_authority/service.cc:313-335`).
  Option 4 adds a URI check. Each is a consensus change Detent consumes.

Why it matters: an operator who follows the design takes a 3-voter Detent group down for the
length of a restart, with no error that names the cause.

Required change:

1. State the restart order for a single renewal (a peer, the renewed voter, then the remaining
   peers) and the schedule and restart count for a full rotation, in the Options section and
   in Q3 option 1.
2. Correct pki:51: Detent revokes by replacing a pin.
3. Mark Q3's options 2 to 4 as consensus interface changes, proposed through Detent's agent
   and decided by its maintainer.
4. If option 3 is proposed, D6 refuses a request whose SPKI is active under another identity.
   Two voters sharing one SPKI pin are otherwise refused only at startup (`tls.h:29-31`),
   after both leaves exist.

#### PKI-R3. Cross-repository touches without an owner (condition)

- Q4 option 2 adds a CRL path to `reactor::tls::Options`, which consumers fill positionally:
  `Options{cert, key, ""}` (`src/tls.cc:39`), Detent's three-field form
  (`Detent/src/detent_authority/service.cc:299`), and reactor's tests, which pass
  `system_roots` as the fourth initializer (`reactor/test/tls_test.cc:136-137`). A new field
  goes last and defaults off, or it silently takes another field's value.
- T6's two-anchor test (pki:274; flag 2, pki:256-258) is a change to reactor's test suite.
  The same test in pki's suite, against `reactor::tls`, changes no reactor file.
- D3's "if completion exposes them as a standalone header" (pki:176-177) is a completion
  interface change. Detent's ledger asks that library files be coordinated with the active
  maintainer first (`Detent/docs/DEPENDENCY_UPGRADES.md:73`).

Condition C1: list these in "Steps" with each one's repository and owner, and put the
two-anchor test in pki's suite.

#### PKI-R4. The authority's records are unversioned formats that cannot drive D7 and D8 (blocking)

Evidence:

- D3 and D6 define the files and "one line per certificate" (pki:164, 205-206) with no
  grammar, no version, and no reader rule. The authority lives 1,826 days (pki:190) and
  outlives many releases of the binary that embeds the library (Q2): a newer `xapiand pki`
  reads what an older one wrote, and an older one must refuse what it cannot read.
- D7's `finish` is "refused while an active line is from the old CA" (pki:217), but no line
  records its issuer (pki:205-206).
- D8 issues "new leaves only for the identities listed in `FILE`" in one restart
  (pki:225-227), with no request round trip. The authority holds no public keys: the index
  keeps digests, and D2's requests leave the keys on the nodes.
- No record keeps the DNS and IP names a leaf carries, so D8 cannot reissue them and
  `pki list` cannot show them.

Why it matters: the rotation commands need data the design does not store, in a format that
nothing versions.

Required change:

1. A version in the directory (a file, or the index's first line); readers refuse an unknown
   version; a fixture per version.
2. A stated grammar for the index line, with its escaping.
3. Each line gains the issuer (the CA's subject key id or DER digest) and the SANs, and each
   issued certificate is kept under `DIR`, as `openssl ca` keeps `new_certs_dir`. D8 then
   recertifies the kept keys; a `--rekey` variant takes fresh requests when node keys may be
   compromised.

### The rest

#### PKI-R5. The CN is a hostname to OpenSSL and curl (blocking)

Evidence (`probes/profile_check.out`, `probes/cn_fallback_tls.out`, OpenSSL 3.6.5 and 4.0.3):

- A leaf with only the URI SAN and `CN=node1` verifies for host `node1`:
  `openssl verify -verify_hostname node1` says OK, and `s_client -verify_hostname node1` says
  "Verify return code: 0 (ok)". A leaf with a URI and an IP SAN also verifies. macOS curl
  8.7.1 accepts the URI-only leaf (HTTP 200) and refuses the one with an IP SAN.
- A leaf with a DNS SAN, or with `CN=xapiand node node1`, is refused (error 62, hostname
  mismatch).
- OpenSSL falls back to the CN when a leaf has no DNS SAN. `bind_hostname` sets no host flags
  (`reactor/reactor_tls.h:97-99`), and Detent's publisher and status clients use it
  (`Detent/docs/LIBRARY_BOUNDARIES.md:250`).

Why it matters: D5 calls the CN "informational only" (pki:195) and makes `--dns` optional
(pki:194). Every `peer` or `client` leaf issued without a DNS name hands its node name, as a
hostname, to every client that trusts the CA.

Required change: an empty subject and a critical SAN. It passes `-x509_strict`, leaves the
host check nothing to fall back to (error 62), and macOS curl accepts it for its DNS SAN, on
3.6.5 and 4.0.3 (`probes/empty_subject.out`). A CN holding the URI or a space also stops the
fallback, but OpenSSL refuses a commonName over 64 characters (`probes/cn_length.out`), which
would cap every name. A `server` leaf needs at least one `--dns` or `--ip`, since a
hostname-checking client cannot verify one without. T1 gains the refusal case.

#### PKI-R6. Operations that write several files have no commit point (blocking)

Evidence:

- Each write is atomic per file (pki:172-174). But `sign` writes a leaf and the index;
  `rotate begin` writes `next.key`, `next.pem`, and `trust.pem`; `finish` renames two files,
  rewrites `trust.pem` and the index, and removes a key; `emergency` does all of that and
  issues leaves.
- A kill between `finish`'s renames leaves the new `ca.key` beside the old `ca.pem`. The next
  `sign` signs with a key its issuer certificate does not match.
- A kill after `sign` writes the leaf and before the index commits leaves a valid certificate
  the index does not know. The next `sign` of that identity succeeds without `--replace`: the
  duplicate D6 exists to prevent.
- T8 asserts "every file old or new and the index parseable" (pki:276), which both states
  pass.

Required change: the index is the commit point (the line first, then the outputs, with the
certificate kept under `DIR` so an output lost to a crash is exported again); each CA
generation lives in its own subdirectory, and a one-line `current` file switches generations,
so `begin`, `finish`, and `emergency` each commit with one rename; every open checks that the
key matches its certificate; T8 asserts the directory as a whole is old, new, or an
intermediate the next command completes. The kill hook exists only in test builds.

#### PKI-R7. A lost authority has no procedure (blocking)

Evidence:

- TS-R8 required "an operator-run escrow export and restore" and "the recovery procedure when
  the authority is lost" (review:339-343). D2 sends TS-R5 to TS-R8 to the deferred enrollment
  design (pki:152-153), but the key lives now in one directory on one operator host, and the
  design never mentions a backup.
- A lost disk, a forgotten passphrase, or a stolen host ends issuance. With 365-day leaves
  every node's TLS ends within a year, and no node can be added from that day. Two restored
  copies that issue in parallel diverge, and D6's uniqueness is lost.

Required change: `pki backup` and `pki restore`, with a generation in the directory's version
record so a restore retires the copy it replaces; a written lost-authority procedure (a new
authority, then new requests or recertification from the stored certificates of PKI-R4, then
distribution as in D8); a row that restores a backup and issues from it, and one that runs the
lost-authority procedure on a 3-node cluster.

#### PKI-R8. Signing leaves the identity to the request (blocking)

Evidence:

- `pki request --name N` writes the request on the node, and
  `pki sign --authority DIR --role R --cluster C node.csr` names no identity (pki:201-203).
  The name comes from the request, which the node, or anyone carrying it, wrote. `--dns`,
  `--ip`, `--days`, and the output path appear in no command.
- `--cluster` on each signing lets one authority mint identities for any cluster; a mistyped
  cluster gives another cluster that trusts the CA a valid leaf.
- Renewal keeps the key (pki:208-209), and an active identity is refused without `--replace`
  (pki:206-208). Every renewal then carries `--replace`, the flag meant to mark a takeover.
- A request's signature proves possession of its key, not that it came from the node it
  names, and nothing lets the operator check that.

Required change:

1. The scheme and the cluster are fixed at `pki init` and kept in `DIR`.
2. `sign` takes `--name`, `--dns`, `--ip`, `--days`, and `--out`; it refuses a request whose
   subject names another identity, and never overwrites an output.
3. A request with the active line's SPKI is a renewal and needs no flag; another key for an
   active identity needs `--replace`; an SPKI active under another identity is refused
   (PKI-R2).
4. `request` prints its SPKI digest, and `sign --expect-spki` checks it: the out-of-band
   comparison. The transit threat itself is the security review's.

#### PKI-R9. D7 restarts Xapiand nodes that load no CA, has no way back, and strands a node that never renews (condition)

- Xapiand's step-1 nodes load no CA: `tls_setup` passes `Options{cert, key, ""}` with the
  Server role (`src/tls.cc:39, 42`), which verifies nothing (`reactor/reactor_tls.h:83-85`).
  Phase 1's "Distribute `trust.pem` to every consumer and restart each" (pki:213-214) restarts
  nodes for nothing. The consumers that need the bundle are API clients, which the operator
  may not control. T6 restarts nodes in all three phases (pki:274).
- No `rotate abort`: an incoming CA made wrong, or exposed during the overlap, has no way
  back.
- The overlap is safe only when every consumer holds the two-CA bundle before phase 2;
  nothing records or checks it.
- A node that never renews keeps an active line on the old CA, so `finish` is refused
  (pki:217), the right default. `--force` then strands it: after phase 3 its clients refuse
  its leaf, and nothing lists it. Between rotations such a node expires at notAfter: Xapiand
  warns 14 days ahead and daily (ts:112-113), refuses to restart once expired
  (`src/tls.cc:49`), and a running node keeps presenting the expired leaf.

Condition C2: phases per consumer (for Xapiand step 1, phases 1 and 3 change clients only);
`rotate abort` before any leaf from the incoming CA, or retiring those leaves; a step that
compares each consumer's bundle fingerprints (D10) before phase 2; `pki list --issuer`;
`--force` retires and prints the lines it strands; T6 asserts what changes on nodes (phase 2)
and on the client.

#### PKI-R10. The authority's clock decides every validity (condition)

Every notBefore and notAfter comes from the authority host's clock (pki:189). An offline host
with a wrong clock issues leaves that are not yet valid, which Xapiand refuses at startup
(`src/tls.cc:48`), or that expire early. The 3,600 s backdate covers nodes up to an hour
behind the authority, not an authority that is wrong.

Condition C3: `sign` refuses when the clock reads earlier than the CA's notBefore or the
index's newest issuance, and prints the issuance time; T5 gains the case.

#### PKI-R11. D4 and D5 fix fields for five years without saying so (condition)

- The CA's subject is unspecified. Two CAs with one subject in one bundle break path building
  for any leaf without an AKID: error 7, signature failure, on 3.6.5 and 4.0.3
  (`probes/same_subject_bundle.out`). D5's leaves carry an AKID and pass; a bring-your-own
  leaf may not.
- `pathlen:0` (pki:190) rules out the online issuing intermediate the Options list
  (pki:93-95) until the next rotation. Name constraints, if ever wanted, also need a new CA.
- Once the CA has under 365 days left, a default `sign` is refused (pki:189, 221-222).
- D4 accepts RSA requests (pki:180-181) without a reason. OpenSSL ignores keyUsage when it
  picks a static-RSA suite: with D5's `digitalSignature`-only RSA leaf and no `strict`, a
  client offering only AES128-GCM-SHA256 negotiates it on 3.6.5 and 4.0.3
  (`probes/rsa_ku_static.out`). With ECDSA leaves no consumer can negotiate one, `strict` or
  not.

Condition C4: a CA subject naming the scheme, the cluster, and a generation; `pathlen:0`'s
consequence and the name-constraints decision recorded in D5 (a critical
`permitted;URI:<cluster>` costs nothing at `init` and binds the CA to its cluster, SR10's
probe); the date refusal names the rotation; RSA requests justified, or refused.

#### PKI-R12. D3's file checks: two wrong claims and a fifth implementation (condition)

- "completion's `PosixIO` has the same refusals" (pki:175-176): it refuses group- or
  world-writable files and directories (`mode & 0022`,
  `completion/include/completion/posix.h:105, 335, 376`), not readable ones. It guards journal
  integrity; D3 guards key confidentiality.
- The threat model's "Consumers refuse a readable key" (pki:137) holds for Xapiand
  (`src/tls.cc:32`), not Detent. Detent checks its configuration file
  (`Detent/src/detent_authority/service.cc:203-206`) and passes the key paths it names to
  `reactor::tls::configure` unchecked (`service.cc:293-300`,
  `Detent/src/detentd/main.cc:258`). `RELAY.md:15` covers the relay's credential files.
- D3 checks the key files, not `DIR`. The "Writer of the authority directory" row (pki:138)
  relies on mode 0700, which the tool sets and never verifies.
- Key-file checks already exist in Xapiand (`src/tls.cc:30-32`, a `stat` followed by a second
  open by path inside reactor), in completion (`posix.h:102-108`), and at four sites in Detent
  (`Detent/src/detent_cli/daemon.cc:58-60`, `detent_relay/host.cc:213-216`,
  `detent_delivery/configuration.cc:64-68`, `detent_authority/service.cc:203-206`). D3 adds
  a fifth.
- Unstated: the scrypt parameters (the OpenSSL CLI picks N = 16,384, r = 8, p = 1:
  `probes/pkcs8_scrypt.out`; SR5 shows it is also the most a stock reader accepts); on macOS
  `fsync` does not flush the drive cache and `F_FULLFSYNC` does (`completion/README.md:25`);
  FAT and exFAT media keep no POSIX modes, so D3's refusals reject every key on them; `flock`
  on network filesystems.

Condition C5: correct both claims; check `DIR`'s owner and mode as `PosixIO` checks its
directory (`posix.h:330-337`); write the check once, as a dependency-free header, and record
the duplication for the owner with a proposed shared home; state N, r, and p; use
`F_FULLFSYNC` on macOS; require a local POSIX filesystem; send the Detent key finding to
Detent's ledger through its agent.

#### PKI-R13. Q4's argument is wrong; the decision holds (condition)

- "A CRL changes nothing until contexts reload without a restart" (pki:231-232, 332-333). A
  CRL takes effect at the next restart, as `trust.pem` does in D7 and D8, and it costs less
  than D8 for one stolen leaf: no leaf is reissued.
- The reasons that hold: no step-1 verifier would load one (Xapiand's nodes verify no client,
  `src/tls.cc:39`; Detent's peers pin; API clients would opt in one by one), and an offline
  authority must re-sign the CRL before every nextUpdate, or every verifier fails closed:
  "CRL has expired", error 12, one hour after nextUpdate (`probes/crl_expiry.out`).

Condition C6: replace the argument, and record the nextUpdate cost and the `Options` change
(PKI-R3) as the deferred CRL design's requirements.

#### PKI-R14. Step 1's scale is unstated (condition)

The design cites strata's 100,000 nodes (pki:37-38) and bounds nothing. Each signing is a
request carried by hand, and the index, about 270 bytes a line, is rewritten whole on every
write (`probes/index_scale.out`):

| Nodes | Leaf life | Signings a day | Index at 5 years | Rewritten a day at 5 years |
|---|---|---|---|---|
| 1,000 | 365 days | 2.7 | 1.5 MiB | 4.2 MiB |
| 1,000 | 90 days | 11.1 | 5.5 MiB | 61 MiB |
| 100,000 | 365 days | 274 | 155 MiB | 41 GiB |
| 100,000 | 90 days | 1,111 | 548 MiB | 594 GiB |

Condition C7: state the node count step 1 serves, from the signings a day an operator can
carry, and that enrollment (Q6) is required past it; add batch signing if the bound is in the
hundreds.

#### PKI-R15. The options omit existing tools (condition)

The Form table (pki:77-82) weighs a library, a Xapiand command, a separate tool, and scripts.
It omits adopting an existing authority tool, such as Smallstep's `step` CLI, cfssl, or
easy-rsa. The reasons to build exist: the profile, the uniqueness index, and the data-root
refusal are the value; both consumers already link OpenSSL 3; those tools add a Go binary or a
shell layer on the authority host. The owner should see the row before a security library is
written.

Condition C8: add the row with its reasons.

#### PKI-R16. Gate-row fixes (condition)

- T1: DER drops a leading zero byte, so 16 random bytes with the top bit clear encode in 15
  bytes 1 time in 256 (exactly 1/256; 3,884 of 1,000,000 sampled; `probes/serial_length.out`).
  A suite that checks 16 bytes on each of eight leaves fails 3.1% of runs. Set bit 126 as well
  (a first byte of 0x40 to 0x7f: always 16 bytes, 126 random bits), or assert 1 to 16 bytes
  with at least 64 random bits. Add PKI-R1's vectors and PKI-R5's refusal.
- T2: `tls_check.sh` signs its RSA, expired, and future leaves with `openssl ca` and
  `$WORK/ec/ca.key` (`harness/tls_check.sh:33-59`). Say whether they stay `openssl`-made
  against a `--no-passphrase` authority laid out as `tls_check.sh` expects. A `pki`-made RSA
  leaf keeps the static-RSA case honest: keyUsage refuses nothing there
  (`probes/rsa_ku_static.out`), so the refusal still comes from `strict`.
- T4: "owned by another user" isolates its variable only when the tool runs as root on a
  fixture another user owns; a non-root caller fails the open with EACCES first. Name the CI
  step. Say whether the eight concurrent signs name eight identities or one (one identity: one
  success, seven refusals). Add PKI-R8's refusals and `DIR`'s mode.
- T5: at exactly notAfter, 3.6.5 refuses and 4.0.3 accepts (`probes/attime_bounds.out`).
  T5's "+1 s" holds on both; put the accept case at notAfter − 1 s, never at notAfter. Add
  PKI-R10's case.
- T6: `cluster_check.sh` curls plain HTTP (`harness/cluster_check.sh:107, 136, 153, 162`) and
  passes no per-node TLS flags. Name the change; scope it per PKI-R9.
- T7: add the accept case: each kept identity's new leaf verifies against the new bundle.
- T9: name each job's OpenSSL (3.0 from an Ubuntu LTS image or FreeBSD 14's base system; 4.0
  from Homebrew's `openssl@4` or a source build). TSan sees races inside one process only: say
  whether the concurrent signs are threads or processes.
- T10: rerunnable, and it catches the likely bug: `X509_pubkey_digest` hashes the key bits
  only (58a93b7a…, against the DER SPKI's 029a19f8…, `probes/spki_digest.out`). Add that the
  printed pin is 64 lowercase hex digits, the form Detent parses
  (`Detent/src/detent_authority/service.cc:320-335`).
- Missing rows: `ca.key` encrypted at rest and a wrong passphrase refused; `DIR`'s owner and
  mode; PKI-R8's identity and renewal rules; `rotate abort`; backup, restore, and the
  lost-authority procedure (PKI-R7); the clock refusal (PKI-R10); and fuzzing of the request
  intake and the index reader, which parse input from outside the authority host (the
  pipeline's rule for parsers: one CPU-hour each at the gate, the corpus replayed as tests).

Condition C9: each item holds in the gate's evidence.

#### PKI-R17. Operations and platforms the design omits (condition)

- `pki init` is never specified: its flags, the passphrase prompt, the version record, and
  the fixed scheme and cluster (PKI-R8).
- `pki show FILE` for a request or a certificate (name, key type, SPKI digest), so the
  operator sees what will be signed.
- `pki list` by issuer, state, and role; a passphrase change; the contents of step 2's user
  docs page: issuance, renewal, rotation, emergency, recovery, and bring-your-own certificates
  under the profile (review:159).
- macOS's `/usr/bin/openssl` is LibreSSL 3.3.6 and cannot read the scrypt-encrypted `ca.key`,
  which OpenSSL 3.6.5 and 4.0.3 read (`probes/pkcs8_scrypt.out`). Recovery instructions name
  OpenSSL 3.
- Optional and cheap now: loading `ca.key` through `OSSL_STORE` lets a `pkcs11:` URI hold the
  CA key later without a format change.

Condition C10: these operations are in revision 1, and the docs page is in step 2.

#### PKI-R18. Review tier, done criteria, and smaller corrections (condition)

- The library is crypto and secrets code. It lands after a code review and a security review,
  as Detent's proposal also requires (`Detent/docs/DEPENDENCY_UPGRADES.md:59`). "Steps"
  (pki:280-286) should say so.
- The ledger has no done criteria yet (`docs/status/pki.md`). Before step 1 starts, the lead
  writes them: T1, T3 to T5, and T8 to T10 with PKI-R16's fixes and missing rows; ASan, UBSan,
  and TSan; CI green on Linux, macOS, and FreeBSD.
- Detent's fixture already sets basicConstraints, keyUsage with keyEncipherment, and both key
  ids (`Detent/tests/detent_authority/tls_certificates.cmake:15`); pki:14-16 reads as if it
  set fewer.
- Flag 1's Kubernetes claim (pki:252-254) fails under `fsGroup`; SR4 has the kubelet
  evidence and the fix.

Condition C11: as stated.

## The security review

`docs/reviews/pki-security.md` (e67ec4875) approves with conditions on revision 1, before
step 1's code. The two reviews reach most findings independently, with their own probes; they
differ on six points.

Where they meet:

| Here | There | Note |
|---|---|---|
| PKI-R1, the role and the canonical form | SR2, SR11 | The same fixes: the role in the path, one encoder with vectors |
| PKI-R5, the CN | SR1 | The same evidence on both builds; the fixes differ (point 3) |
| PKI-R7, the lost authority | SR8 | SR8 adds what this review lacks: D8 runs on a fresh host, and the overlap's two live CA keys need a warning |
| PKI-R8, signing | SR6, SR10 | SR6's parse-before-the-passphrase rule complements PKI-R16's fuzz row |
| PKI-R11, RSA requests | SR9 | The same probe result |
| PKI-R12, `DIR` and the scrypt parameters | SR7, SR5 | SR7's `openat` and `O_EXCL` rules and SR5's reader limit are the specifics |
| PKI-R16, T5 and T10 | SR12 | The same traps |
| PKI-R18, flag 1 | SR4 | SR4 reads the kubelet sources; this review defers to it |

Where they differ:

1. The verdict. SR's conditions on revision 1 are this review's blocking items under another
   name: both hold step 1's code until revision 1. This review also blocks on what a threat
   model does not cover: formats that freeze (PKI-R4), multi-file atomicity (PKI-R6), and a
   procedure that loses quorum (PKI-R2).
2. SR12's serial rate. A 15-byte serial needs a first byte of 0x00 and a second byte below
   0x80; when the second byte's top bit is set, DER keeps the zero. The rate is 1 in 256, not
   1 in 128 (`probes/serial_length.out`: exactly 1/256, 3,884 of 1,000,000 sampled). The fix
   is the same.
3. SR1's fixes. A CN holding the URI meets OpenSSL's 64-character commonName bound: 65
   characters are refused with `maxsize=64` on 3.6.5 and 4.0.3 (`probes/cn_length.out`), so
   `xapiand://production-east/node/` leaves 33 characters for a name. An empty subject with a
   critical SAN has no such bound (PKI-R5). Setting `X509_CHECK_FLAG_NEVER_CHECK_SUBJECT` in
   `bind_hostname` is sound defense in depth, but it changes reactor for every consumer: a
   server certificate that names its host only in the CN stops verifying for Detent's
   publisher and status clients and for any other `bind_hostname` caller. It is an interface
   change, opt-in or coordinated with Detent, as `strict` was.
4. SR3's 90-day leaf (Q5). D6 renews with the same key (pki:208-209), and a leaf is public:
   a thief holding a node's private key presents each renewed leaf the node itself serves, so
   a shorter life does not bound an undetected theft. It bounds only the tail after the
   operator re-keys the node, while each renewal costs a restart (D1.7) and, with DER pins, an
   ordered pin change on every Detent voter (PKI-R2). A short life bounds theft only when
   every renewal re-keys, which needs a new request per renewal and changes SPKI pins, against
   Q3 option 3. SR3's other fixes stand: the exposure in days, SANs recorded, shared names
   warned.
5. Detent's key files. SR4 proposes Detent's "controlled 0640" for Xapiand's keys, which is
   sound. Detent itself applies that rule to the relay's credential files (`RELAY.md:15`) and
   checks none of its TLS keys (PKI-R12), so the design's "Consumers refuse a readable key"
   (pki:137) fails on Detent. SR13's threat-model corrections do not record it; it is a new
   item for Detent's ledger.
6. SR10's `sign --cluster` override with a warning. One authority per cluster is simpler,
   and with SR10's own `permitted;URI:<cluster>` constraint in the CA every OpenSSL verifier
   refuses an overridden leaf anyway (SR10's probe), so the override buys nothing.

## Open questions

1. Q1, where it lives: option 1, a new `Kronuz/pki`. TS-R17 rules out reactor, and a
   Xapiand-only copy puts the profile where Detent cannot reuse it. The framing is honest;
   PKI-R15's row belongs beside it.
2. Q2, the command: option 2, `xapiand pki`, with the entry point in the library target, so
   every consumer runs the same code, and a subcommand that fixes the scheme. Say that
   `kronuz-pki` is built only in the library's CI, as its test driver and example, and not
   packaged; otherwise the recommendation is option 3.
3. Q3, Detent's pin: option 1 now, with PKI-R2's restart order written down. Propose option 3
   in Detent's ledger: a leaf renewal becomes one restart, and a 3-voter phase 2 three
   restarts in any order, against at least five in one schedule. It needs PKI-R8's SPKI rule.
   Option 4 is strata's question (PKI-R1).
4. Q4, revocation: option 1, with PKI-R13's corrected reasons.
5. Q5, leaf life: option 1, 365 days. Each renewal is a hand-carried request (2.7 signings a
   day at 1,000 nodes, against 11.1 at 90 days: `probes/index_scale.out`), a restart (D1.7),
   and for Detent an ordered pin change (PKI-R2). Under same-key renewal a shorter life
   bounds only the tail after a re-key, not an undetected theft (point 4 under "The security
   review").
   State the exposure in days, as SR3 asks; revisit with enrollment and reload.
6. Q6, online enrollment: option 1, deferred to strata's peer transport; TS-R1 and TS-R7
   still hold.

## Required changes

1. PKI-R1: the role in the URI's path; a canonical-form algorithm with vectors, or SPIFFE;
   the strata identity question put to the owner.
2. PKI-R2: Detent's safe restart order and rotation schedule written down; pin revocation
   corrected; Q3's options marked as consensus interface changes.
3. PKI-R4: a versioned directory with a stated grammar; the issuer, the SANs, and the issued
   certificate kept for every line.
4. PKI-R5: an empty subject with a critical SAN (or a CN that cannot be a hostname, within
   64 characters); a `server` leaf needs a DNS or IP SAN.
5. PKI-R6: one commit point per multi-file operation; T8 asserts the directory's state as a
   whole.
6. PKI-R7: backup, restore, and the lost-authority procedure, with rows.
7. PKI-R8: the scheme and cluster fixed at `pki init`; the identity, SANs, life, and output
   given at signing; renewal recognized by SPKI without `--replace`; the SPKI checked at
   signing.

## Conditions

1. C1 (PKI-R3): the cross-repository items with owners; the two-anchor test in pki's suite.
2. C2 (PKI-R9): per-consumer phases, `rotate abort`, a bundle check before phase 2,
   `list --issuer`, `--force` that retires and prints, and T6 rescoped.
3. C3 (PKI-R10): the clock refusal and its T5 case.
4. C4 (PKI-R11): the CA subject, `pathlen:0`'s consequence, name constraints, the date
   refusal's message, and RSA requests justified or refused.
5. C5 (PKI-R12): both claims corrected; `DIR` checked; one key-file check; the scrypt
   parameters; `F_FULLFSYNC`; a local POSIX filesystem; Detent's key finding in its ledger.
6. C6 (PKI-R13): Q4's argument replaced and the CRL requirements recorded.
7. C7 (PKI-R14): step 1's node bound.
8. C8 (PKI-R15): the existing-tools row.
9. C9 (PKI-R16): the gate-row fixes and the missing rows.
10. C10 (PKI-R17): the omitted operations and the LibreSSL note.
11. C11 (PKI-R18): the review tier, the done criteria, and the corrections.

Not run: no build, compile, or test of Xapiand or any library; no code exists for this design.
`quiet check` returned 0 before the probes; on resuming, the strata gate VM run held the host
strictly (exit 1, until 01:17), so the last probes are openssl commands and one loopback
`s_server`, and nothing was built. Left unbuilt: consensus's `authenticated_voter` for T3 (its
DER pin is `X509_digest`, `tls.h:12-20`), and a reactor build for the two-anchor case (SR12's
compiled probe covers it). No benchmark or fuzzing. No Linux or FreeBSD host: their OpenSSL
versions are not checked, and T9's matrix is judged from the design. No Kubernetes cluster.
`gh repo list Kronuz` was not run; the local checkouts were searched instead. Dogtag's `pki`
command (pki:310) was not checked. The quorum model is a model of the cited consensus code,
not a run of Detent. The security review was read after this review's findings were written,
and its probes were not rerun. The review wrote only this file and `.scratch/review-pki/`,
plus one private claude.ai copy of an earlier draft, reported to the lead.
