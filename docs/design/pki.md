# PKI helper

Status: draft, revision 0, for preliminary review. No code exists. The decisions below
stand until the review and the owner's answers to the open questions.

## Why

Both consumers verify certificates and neither makes them. Xapiand serves its REST API
over TLS from `--tls-cert` and `--tls-key` (`docs/design/transport-security.md:62-65`)
and leaves where they come from to the operator. Detent requires mutual TLS on its
authority peer transport and HTTPS for publishers (`Detent/docs/AUTHORITY_TRANSPORT.md:5`),
pins each voter's leaf (`AUTHORITY_TRANSPORT.md:13`), and does not generate a CA. Today
an operator hand-runs `openssl`, and each project carries its own script with its own
profile: Xapiand's `harness/tls_certs.sh:13-19` (P-256, a leaf with no `keyUsage` or
`basicConstraints`), and Detent's `tests/detent_authority/tls_certificates.cmake:12-23`
(RSA 2048, `serverAuth,clientAuth`, `DNS:localhost`). Both are test fixtures; neither is a
procedure an operator can follow, rotate with, or audit.

The helper makes one cluster authority, issues node certificates under one profile, and
rotates both, offline, so an operator gets TLS without learning `openssl x509 -req`.

## What each consumer needs

### Xapiand

| Need | Today | Source |
|---|---|---|
| A server certificate for the API, with DNS and IP SANs that clients verify | PEM chain and key, key mode 0600 or stricter, valid now; a warning within 14 days of expiry | `transport-security.md:62-65, 107-113`; `src/tls.cc:30-32, 46-53` |
| A CA file for clients | `curl --cacert`, newman `--ssl-extra-ca-certs` | `transport-security.md:141` (T1) |
| Client certificates | None: no reactor role verifies a client optionally, so the API asks for none | `transport-security.md:64-65`; `reactor/reactor_tls.h:48-51` |
| Node certificates for mutual TLS | Deferred to strata's consensus peer transport, with one SAN profile binding cluster and node name, unique at issuance | `transport-security.md:125, 128`; review TS-R2 (`docs/reviews/transport-security-design.md:129-160`) |
| Rotation | A restart per certificate change; no reload | `transport-security.md:115-116` (D1.7), `:131` (D8 deferred) |
| Revocation | None | — |

Under strata, a member's transport identity (address and certificate pin) comes from a node
directory keyed by node id, and pins rotate there without a `Configuration` entry
(`strata/docs/design/track-c.md:211-215, 251`). Strata targets 100,000 nodes or more
(`strata/docs/PLAN.md:424`).

### Detent

| Need | Today | Source |
|---|---|---|
| A cluster CA | `peer_tls` and `publisher_tls` each name `certificate`, `key`, and `ca`; replicated configuration requires both | `AUTHORITY_TRANSPORT.md:47` |
| Peer certificates, both ends presenting one | reactor's `Peer` role: `verify_peer` and `verify_fail_if_no_peer_cert` | `reactor/reactor_tls.h:83-85`; `AUTHORITY_TRANSPORT.md:13` |
| Peer identity | A SHA-256 of each voter's leaf DER, one per voter, unique; the local certificate must match the local pin. "A trusted CA alone cannot give an arbitrary certificate holder a voter identity." | `AUTHORITY_TRANSPORT.md:13, 47`; `consensus/include/consensus/transport/tls.h:21-36, 45-66` |
| Publisher, relay, and status certificates | A DNS name or IP SAN the client verifies (`publisher_name`, default `localhost`; the relay's `hostname` must match its SAN) | `AUTHORITY_TRANSPORT.md:13, 47`; `Detent/docs/RELAY.md:65` |
| A CA file for agents | Certificate verification is required | `Detent/docs/USAGE.md:530` |
| Credential files | Regular files, one link, no symlinks, mode 0600 or controlled 0640 | `RELAY.md:15` |
| Rotation | A pin change on every voter's configuration, and a restart | `AUTHORITY_TRANSPORT.md:47`; `tls.h:24-35` |
| Revocation | Removing a voter's pin refuses that voter; nothing else | `tls.h:47-66` |

Detent's maintainer proposals already list generated certificates as a later item: a
`reactor::pki` with a first-start CA, signed node certificates, and single-use enrollment
tokens, "Xapiand qualifies it first; reviewed as security code before any adoption"
(`Detent/docs/DEPENDENCY_UPGRADES.md:59`).

### What reactor's roles already give

`reactor::tls::configure` (`reactor/reactor_tls.h:56-86`, at `e19afd9`) sets TLS 1.2 or
later, no renegotiation, no 0-RTT, and a 64 KiB certificate-list cap for every role. The
`Server` role presents a certificate and verifies none; `Peer` requires one from both ends;
`Client` verifies the server and may present one (`:48-51`). The authority is one PEM file
loaded with `load_verify_file` (`:81`), which loads every certificate in it, so a bundle of
two CAs is trusted as two anchors. `bind_hostname` (`:88-101`) checks a DNS name or IP
literal. Nothing in reactor checks a URI SAN, loads a CRL, or swaps a context.

The helper therefore needs no reactor change to serve either consumer today. The peer
identity check that strata needs belongs in `reactor::tls` beside `bind_hostname`
(TS-R17, review `:578-583`); this design defines the profile that check reads, not the
check.

## Options

### Form

| Option | What it is | For | Against |
|---|---|---|---|
| A. Library | OpenSSL 3 calls behind a small C++ API: make an authority, sign a request, rotate, read the index | Testable in CI on every platform; usable later by an in-cluster enrollment | An operator cannot run a library |
| B. A command in Xapiand | `xapiand pki init\|request\|sign\|rotate`, in the server binary | One binary already packaged for Docker, Homebrew, FreeBSD | Detent would depend on Xapiand, or copy it |
| C. A separate tool | A library plus a command built from it, in its own repository | Both consumers ship it; one profile; one test suite | A new repository to maintain (Q1) |
| D. Documentation and scripts | A guide and a shell script over `openssl` | No new code; `openssl` is everywhere | The profile lives in shell; no uniqueness, lock, or atomic write; the two existing scripts already disagree |

The library is the mechanism and the command is its operator surface; they are one
choice, not two. D is what exists today, twice.

### Where the authority key lives

1. **Offline, on an operator host.** The key never reaches a serving node. Issuing needs
   the operator.
2. **In a node's data directory.** Revision 0's D5. Every backup and volume copy carries
   it, and the process that parses every HTTP request holds it (TS-R8, review `:318-345`).
3. **On an online issuing intermediate**, with an offline root (review `:95`). Needs an
   enrollment listener and protocol (TS-R7), which do not exist on either consumer.

### Bootstrap of a new node

1. **Manual copy.** The authority issues a key and a certificate; the operator copies them.
   The key travels.
2. **A certificate request.** The node makes its key and a request; the operator carries
   the request to the authority and the certificate back. The key never travels; neither
   file is secret.
3. **A join token and enrollment over the cluster.** Unattended, but it needs a listener
   that serves nodes without certificates, a token bound to the authority's key and the
   new node's name, an atomic persisted consume, and a request policy (TS-R7, review
   `:279-316`). Neither consumer has the listener; strata's transport is where it lands.

### Rotation without downtime

A leaf is renewed by issuing a new one and restarting the node (Xapiand D1.7; Detent has
no reload either). An authority is rotated by trusting two CAs during an overlap: every
consumer's CA file holds the old and new certificates, then leaves move to the new CA,
then the old CA leaves the file. Cross-signing the new CA with the old avoids distributing
the bundle twice but puts two chains on the wire and makes path building depend on the
verifier; it is not chosen.

Detent's pins rotate separately from its CA. A renewed leaf has a new DER and so a new pin,
and each voter accepts exactly one pin per voter (`tls.h:26, 29-31`). Probe
(`.scratch/pki-probe`, OpenSSL 4.0.3): one key signed by two CAs gives two DER digests
(`5aa4a341…`, `3a3769d5…`) and one SPKI digest (`635f6ca8…`). A voter whose leaf changes is
refused by every peer still configured with the old pin until that peer is restarted with
the new one: with 3 voters a rolling change keeps quorum (2 of 3) but runs without fault
tolerance until the last restart.

## Threat model

Options: A+C offline (the recommended form, an offline authority, certificate requests), B
(the same in Xapiand's binary), D (scripts), and E (an online authority with enrollment).

| Actor | Capability | Stopped by | Residual |
|---|---|---|---|
| Network, passive or active | Read, tamper, impersonate a node | Every option: TLS with chain and name verification (D5); peers verify both ends | Traffic analysis; a client that skips verification gets confidentiality against a passive observer only (`transport-security.md:35-36`) |
| Holder of one node's key | Impersonate that node until its leaf expires or the authority rotates | A, B, C, D: names in the SAN limit it to that node's names for hostname-checking clients; Detent's pins limit it to that voter | With CA-only identity and no SAN identity check (a future peer transport without the reactor check), any node; hence D5's identity URI. Under E, it also renews and redeems (TS-R5) |
| Holder of the authority key | Mint any identity; impersonate any node to a client that trusts the CA | Offline storage, a passphrase (D3); emergency rotation (D8) | Every CA-verifying client is exposed until it holds the new bundle. Detent's peers also need a pinned leaf (`tls.h:45-46`), so pins blunt it there. Under E, compromising the issuing node is this row |
| Reader of a node's backup or volume copy | Every file in the data directory | A, B, C: the authority never lives there, and the tool refuses a directory under a Xapiand data root (D3) | A node key the operator stored in the data directory is that node's key (row 2) |
| Unprivileged user on the authority host | Read `ca.key` | Modes 0700 and 0600, refusal to open a looser key, a symlink, or a second link, and the passphrase (D3) | A local root, or a user who learns the passphrase |
| Unprivileged user on a node | Read the node key | Consumers refuse a readable key (`src/tls.cc:32`; `RELAY.md:15`); the tool writes 0600 (D3) | Root on the node is the node |
| Writer of the authority directory | Replace `trust.pem` or the index | Directory mode 0700 | Whoever writes it controls trust; the operator verifies the bundle's fingerprints at distribution (D10) |
| Operator error | An expired leaf or authority; a leaf that outlives its CA; a duplicate identity | Lifetime clamps, the uniqueness index (D6), `pki list --expiring` (D11) | An operator who never runs the check; Xapiand still warns at 14 days (`transport-security.md:112-113`) |
| Under D only | All of the above rows' checks | Nothing enforces them | The profile is whatever the script says; two scripts already differ |

## Decisions

**D1. A library and its command, in their own repository.** A small MIT library over the
OpenSSL 3 API (target `pki::pki`; OpenSSL 3.0 is the floor, as for `reactor::tls`) and one
command built from it (name in Q2). Not `reactor::pki`: issuance is policy, and reactor
stays a runtime (TS-R17). The serving process never runs issuance code; a command
invocation is its own process.

**D2. Offline only in step 1.** The authority lives on an operator host. Bootstrap is a
certificate request (D6). There are no join tokens, no enrollment listener, and no online
intermediate in step 1. Enrollment returns with the deferred D5 on strata's peer
transport, with TS-R5 to TS-R8 as its requirements (Q6).

**D3. The authority directory.** Given by `--authority DIR`, with no default.

| File | Mode | Content |
|---|---|---|
| `DIR/` | 0700 | — |
| `ca.key` | 0600 | The CA's PKCS#8 key, encrypted with AES-256 under a scrypt-derived key unless `--no-passphrase` (for test fixtures) |
| `ca.pem` | 0644 | The current CA certificate |
| `next.key`, `next.pem` | 0600, 0644 | The incoming CA, during a rotation only |
| `trust.pem` | 0644 | The bundle every consumer loads as its CA file |
| `issued` | 0600 | The index, one line per certificate (D6) |
| `lock` | 0600 | Held with `flock` for any write |

The passphrase comes from the terminal or from `--passphrase-file`, never from argv or the
environment; secrets are never printed or logged (`Detent/docs/ASYNC_AUTHORITY_ARCHITECTURE.md:69`).
The tool refuses to open `ca.key` or `next.key` when it is not a regular file, is a
symlink, has more than one link, is not owned by the caller, or is readable by group or
others. It refuses a directory that is, or is under, a directory holding `.xapiand/node`
(Xapiand's data root marker, `src/manager.cc:292`). Every write is a new file in `DIR`,
`fsync`, `rename`, and an `fsync` of `DIR`; modes are set explicitly, whatever the umask.
The file checks are POSIX calls (`open` with `O_NOFOLLOW`, `fstat`), about 60 lines.
completion's `PosixIO` has the same refusals (`completion/README.md:22-23`) inside an
asynchronous I/O library that neither the tool nor Xapiand otherwise needs; if completion
exposes them as a standalone header, the library uses that instead.

**D4. Keys and signatures.** The tool generates ECDSA P-256 keys and signs with SHA-256.
A request may carry a P-256, P-384, or RSA key of 2,048 to 4,096 bits; anything else is
refused. The CA is P-256.

**D5. The certificate profile.** The authority sets every extension; a request supplies only
its public key and proof of possession, and its extensions are ignored.

| Field | Value |
|---|---|
| Version, serial | v3; 16 random bytes with the top bit clear (positive, under RFC 5280's 20 octets) |
| Validity | notBefore is issuance less 3,600 s (a joiner's clock that lags, TS-R7); notAfter is `--days` after issuance, default 365, refused past the CA's notAfter |
| CA | `basicConstraints=critical,CA:TRUE,pathlen:0`; `keyUsage=critical,keyCertSign,cRLSign`; subject key id; default life 1,826 days (5 years) |
| Leaf | `basicConstraints=critical,CA:FALSE`; `keyUsage=critical,digitalSignature`; subject and authority key ids |
| Roles | `server`: `extendedKeyUsage=serverAuth`; `peer`: `serverAuth,clientAuth`; `client`: `clientAuth` |
| Identity | One URI SAN `<scheme>://<cluster>/node/<name>`: the scheme names the consumer (`xapiand`, `detent`), cluster and name lowercased and percent-encoded outside RFC 3986's unreserved set; no wildcards; exact byte comparison; no CN fallback (TS-R2, review `:154-158`) |
| Names | DNS and IP SANs from `--dns` and `--ip`, for clients that check a hostname (`bind_hostname`); a `*` is refused |
| Subject | `CN=<name>`, informational only |

Xapiand node names compare lowercased (`src/node.h:139-142`); Detent's voter ids are
numbers. The URI is ignored by every verifier today and read by the reactor check strata
adds.

**D6. Issuance and uniqueness.** `pki request --name N` on the node writes `node.key`
(0600) and `node.csr`. `pki sign --authority DIR --role R --cluster C node.csr` checks the
request's signature, caps it at 16 KiB, checks the key (D4), and writes `node.pem`.
`pki issue` does both on the authority host for test fixtures and development, and says
that the key travels. The index line holds the serial, identity URI, role, SPKI SHA-256,
DER SHA-256, notBefore, notAfter, and state (`active`, `replaced`, `retired`). Signing an
identity that has an active line is refused unless `--replace`, which marks the old line
`replaced`. Renewal is a new request with the same key (`pki request --key node.key`),
which keeps the SPKI digest.

**D7. Planned rotation.** Three phases, each a command and a distribution:

1. `pki rotate begin`: makes `next.key` and `next.pem`; `trust.pem` holds both CAs.
   Distribute `trust.pem` to every consumer and restart each.
2. `pki sign` issues from the incoming CA from now on. Renew every leaf and restart each
   node.
3. `pki rotate finish`: refused while an active line is from the old CA, unless `--force`;
   `next.*` becomes `ca.*`, the old key is removed, `trust.pem` holds the new CA. Distribute
   and restart.

The overlap is as long as phases 1 and 2 take; nothing expires it. Begin when the CA has a
year left; `pki list` warns from then.

**D8. Emergency rotation.** `pki rotate emergency --keep FILE` (TS-R5): a new CA, no
overlap, `trust.pem` holds only it, every old line `retired`, and new leaves only for the
identities listed in `FILE`. Every consumer gets the new bundle and leaf in one restart; a
node not on the list cannot connect.

**D9. No revocation in step 1.** Neither consumer checks a CRL or OCSP, and reactor loads
none (`reactor_tls.h:56-86`). A stolen leaf key stays valid until its notAfter, the
emergency rotation, or, for a Detent voter, removal of its pin. A CRL helps only once
contexts reload (Xapiand's deferred D8); Q4.

**D10. Fingerprints.** `pki fingerprint FILE` prints the SHA-256 of the DER, which is
Detent's voter pin (`AUTHORITY_TRANSPORT.md:47`; `tls.h:12-19`), and of the SPKI. `sign`
prints both for the leaf it writes, and `rotate` prints the CA's, so an operator checks the
bundle that reaches a consumer.

**D11. Expiry visibility.** `pki list --expiring 30d` prints the index's active leaves and
CAs within 30 days of notAfter and exits nonzero when there are any; `pki check FILE
--within 30d` does the same for one certificate, for a cron job on a node. The consumers
keep their own startup checks (`src/tls.cc:48-53`).

**D12. Step 2: the Xapiand harness adopts it.** `harness/tls_certs.sh` becomes a call to
`pki issue`, and transport-security's T1 and T2 rerun on its certificates. Detent's
adoption is its maintainer's (Q3).

## Flags from the reading

1. **A latent difference in key-file checks.** `src/tls.cc:30-32` uses `stat`, so it
   follows a symlink, and checks the mode only; Detent refuses symlinks and second links
   and allows a controlled 0640 (`RELAY.md:15`). Following symlinks is what Kubernetes
   secret volumes need (they mount through `..data` symlinks, and need `defaultMode: 0600`,
   since the default 0644 is refused), so the server-side check should stay; the tool's stricter refusals apply to the authority key only, and the files
   it writes (0600, one link) pass both consumers.
2. **The trust bundle is untested in reactor.** `load_verify_file` loads every certificate
   in the file, and `openssl verify -CAfile` with a two-CA bundle accepts leaves of both
   (probe above), but no reactor test covers two anchors. T6 adds one.
3. **Detent's proposal names `reactor::pki`** (`DEPENDENCY_UPGRADES.md:59`); TS-R17 put
   issuance outside reactor. Updating that row is Detent's maintainer's.

## Gate rows

A new `harness/pki_check.sh` in Xapiand runs the rows marked "harness"; the library's own
tests run the rest. Every refusal sits beside an accepted case differing in one variable.

| Row | Target | Procedure |
|---|---|---|
| T1 Profile | Every role's leaf passes `openssl verify -x509_strict -purpose sslserver` (server, peer) and `-purpose sslclient` (peer, client) against `trust.pem`; `openssl x509 -noout -text` shows exactly D5's extensions; the serial is 16 bytes and positive; notBefore is issuance less 3,600 s, within 5 s; a request carrying `CA:TRUE`, an extra SAN, and `keyUsage=keyCertSign` yields none of them | Library test; harness `profile` |
| T2 Xapiand handshakes | transport-security T1 and T2 pass on `pki` certificates with the same assertion counts as on `tls_certs.sh`; `curl --cacert trust.pem` verifies `localhost` and `127.0.0.1`, and a name not in the SANs is refused | `harness/e2e_capture.sh` with `CA=trust.pem`; harness `xapiand` |
| T3 Peer handshakes | reactor `Peer` at both ends with `peer` leaves: accepted; a `server` leaf presented as a client: refused; a `client` leaf presented as a server: refused; a leaf of another authority: refused; consensus's `authenticated_voter` returns the configured node for the DER pin `pki` printed, and refuses a renewed leaf until the pin changes | Library test over a memory BIO pair, in the pattern of `reactor/test/tls_test.cc`, linking `consensus::transport` |
| T4 Issuance refusals | Each refused, naming the reason: an active identity without `--replace`; `--days` past the CA's notAfter; a `*` in a name or DNS SAN; a bad request signature; a request over 16 KiB; an RSA 1,024 or P-521 key; `ca.key` group-readable, a symlink, with two links, or owned by another user; an authority directory under a `.xapiand/node` root. Accepted beside each: the same with the one variable fixed. Eight concurrent `sign` calls give eight distinct serials, eight index lines, and one active line per identity | Library test; harness `refusals` |
| T5 Expiry and clocks | A leaf verifies at `-attime` issuance less 3,599 s and is refused at less 3,601 s; refused at notAfter plus 1 s; `pki list --expiring 30d` lists exactly the leaves within 30 days and exits 1, and exits 0 with none | `openssl verify -attime`; harness `expiry` |
| T6 Planned rotation | A 3-node Xapiand cluster serving HTTPS and a client requesting each node every 100 ms with `--cacert trust.pem` through D7's three phases: zero verification failures in any phase, and connection refusals during each restart counted and stated; after `finish`, an old-CA leaf is refused; a client still holding only the old CA refuses a new leaf. reactor's `Peer` role with a two-CA `trust.pem` accepts leaves of both CAs | Harness `rotation` over `harness/cluster_check.sh`'s nodes; a reactor test with a two-anchor bundle (flag 2) |
| T7 Emergency rotation | After `rotate emergency --keep`, `trust.pem` holds one CA; every old leaf is refused by `openssl verify` and by curl; the index's active lines are exactly the kept identities | Harness `emergency` |
| T8 Files | Under umask 000 and 077 alike: directory 0700, keys and index 0600, certificates and bundle 0644; a kill at each step between write and rename (a test hook, `PKI_TEST_KILL_AT`) leaves every file old or new and the index parseable; the passphrase and key bytes appear in no captured stdout or stderr, and no secret is in argv | Library test; harness `files` |
| T9 Platforms | The library's tests pass on Linux x86_64 and arm64, macOS, and FreeBSD 14, against OpenSSL 3.0 and the current 4.0, and under ASan, UBSan, and TSan (TSan for the concurrent `sign`) | The library's CI |
| T10 Fingerprints | `pki fingerprint`'s DER digest equals `openssl x509 -outform DER \| openssl dgst -sha256` and consensus's `certificate_fingerprint`; its SPKI digest equals `openssl pkey -pubin -outform DER \| openssl dgst -sha256`; a renewal with the same key keeps the SPKI digest and changes the DER digest | Library test; harness `fingerprints` |

## Steps

1. The library and command, in their repository (Q1), with T1, T3 to T5, T8 to T10.
2. Xapiand: the harness adopts it (D12), the command ships in the Docker image, the Homebrew
   formula, and the FreeBSD package (Q2), and the user docs gain a TLS operations page; T2,
   T6, T7. A changelog bullet under `[Unreleased]`.
3. Detent: its maintainer's decision (Q3).

## Open questions for the owner

**Q1. Where does it live?**

1. A new public repository `Kronuz/pki`, MIT, library and command, under the
   deliverables standard (`KRONUZ_REFACTORING_DIRECTIVES.md` §6).
2. Inside reactor as `reactor::pki`, as Detent's proposal says.
3. Inside Xapiand only (`src/pki/`), with Detent copying it or going without.

Recommendation: 1. TS-R17 rules out 2, and 3 duplicates the profile in the first consumer
that is not Xapiand. No Kronuz library makes or signs certificates today
(`gh repo list Kronuz`; `cpp-libraries/AGENTS.md`), so it extends nothing.

**Q2. What is the command called, and who ships it?**

1. A standalone `kronuz-pki` from the library's repository, packaged by each consumer.
2. A subcommand of each consumer's binary (`xapiand pki ...`) calling the library's
   command entry point.
3. Both.

Recommendation: 2 for Xapiand, with the library's `kronuz-pki` as its example program.
One binary is already packaged on three platforms, and the subcommand is its own process
(D1). A bare `pki` would collide with Dogtag's `pki` command on Fedora and RHEL.

**Q3. Does Detent adopt it, and with which pin?** Detent's maintainer decides.

1. As is: Detent's configuration takes `pki` files and the DER pins `pki` prints; a leaf
   renewal is a pin change on every voter and a rolling restart without fault tolerance
   (Options, Rotation).
2. consensus accepts a second pin per voter during an overlap.
3. consensus pins the SPKI digest, which survives a renewal with the same key; Detent's
   configuration format changes.
4. consensus accepts D5's identity URI under a dedicated cluster CA instead of pins.

Recommendation: 1 now, which needs no code; propose 3 in Detent's ledger as a library-side
candidate, since it removes the pin change from every renewal at 32 bytes per voter. 4 is
the shape strata's 100,000-node directory may want, and is decided there.

**Q4. Revocation.**

1. None in step 1 (D9).
2. A CRL file: `pki revoke`, a `crl.pem`, and a reactor option that loads it with
   `X509_V_FLAG_CRL_CHECK`.

Recommendation: 1. A CRL changes nothing until contexts reload without a restart (Xapiand's
deferred D8), and emergency rotation covers compromise.

**Q5. Default leaf life.**

1. 365 days.
2. 90 days.

Recommendation: 1 while renewal is manual; 90 days when enrollment renews automatically.

**Q6. Online enrollment.**

1. Defer it to the deferred D5, on strata's peer transport (D2).
2. Design it now, against Xapiand's current remote and replication listeners.

Recommendation: 1. Those listeners are raw-fd paths strata replaces (TS-R1), and an
enrollment listener needs a role reactor does not have (TS-R7).
