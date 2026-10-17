# PKI helper

Status: revision 1, for the preliminary re-check. It folds in the architect's preliminary
review (`docs/reviews/pki-design.md`: REJECT; PKI-R1 to PKI-R18), the security review
(`docs/reviews/pki-security.md`: APPROVE WITH CONDITIONS; SR1 to SR13), and two owner
decisions from the system review (`strata/docs/reviews/system-2026-10-17.md`): SYS-2, peers
authenticate by SPKI pins in one peer transport design (`strata/docs/PLAN.md:26-29`), and
SYS-13, a key file at 0640 passes only with the service's own group (recorded in the TLS
gate's ledger, `tls/d1-gate` at 93bc2448e). No code exists, and the decisions stand until the
re-checks and the owner's answers to the open questions. Q1 decides where step 1 lives and Q7
holds D5's identity code; nothing else holds step 1.

Sources, at the commits read: Xapiand a37a511ae, whose source matches master's 7e8cccb7d
outside `docs/`; reactor e19afd9, Xapiand's pin; consensus ef30b4f, whose `transport/`
headers are unchanged since fc66c0e; completion 5251cf9; io 1336f85; char-classify 306ecd9;
strata 5660f82; Detent 488522d, read only. At 488522d the rows of Detent's
`docs/DEPENDENCY_UPGRADES.md` that the reviews cite as `:57` and `:59` are at `:77` and
`:79`. `review:N` is line N of `docs/reviews/transport-security-design.md`; `probes/` is the
architect's `.scratch/review-pki/probes/` and `sprobes/` the security reviewer's
`.scratch/review-pki-security/`. Below, `pki` stands for the command, whose name is Q2's.

## Changes from revision 0

| Finding | What changed | Where |
|---|---|---|
| PKI-R1 | The role in the identity's path; a SPIFFE ID, which has one byte form, with vectors; the URI advisory; strata's peer identity decided by the owner (SYS-2) | D5, T13, Q7 |
| PKI-R2 | Detent's safe restart orders and rotation schedule; revocation by pin replacement; consensus interface changes marked; one SPKI per identity | Options, Detent's table, D6, D7, Q3 |
| PKI-R3 | Every cross-repository change with its repository and owner; the two-anchor test in pki's own suite | Steps, T3 |
| PKI-R4 | A versioned index with a grammar, holding every certificate the authority issued, and so each one's issuer and SANs; emergency recertifies from them, or takes fresh requests with `--rekey` | D3, D8 |
| PKI-R5 | An empty subject with a critical SAN; a `server` leaf needs a DNS or IP name | D5, T1, T4 |
| PKI-R6 | The index is the one commit point of every operation; the index names every key; every CA key is checked against its certificate; T8 checks the whole directory | D3, T8 |
| PKI-R7 | Backup, restore, import, the lost-authority procedure, and their rows | D8, D13, T11, T12 |
| PKI-R8 | Consumer and cluster fixed at `init`; `sign` takes the name, the DNS and IP names, the life, and the output; renewal recognized by SPKI; `--expect-spki` | D6, D14, T4 |
| PKI-R9 | Steps per consumer; `rotate switch` and `rotate abort`; `--force` retires and prints; `list --issuer`; T6 rescoped | D7, D14, T6 |
| PKI-R10 | `sign` refuses a clock behind the index | D6, T5 |
| PKI-R11 | A distinct CA subject; `pathlen:0`'s consequence; a URI name constraint at `init`; the date refusal names the rotation; RSA only with `--allow-rsa` | D4, D5 |
| PKI-R12 | Both wrong claims corrected; `DIR` checked; one private-file check, its home put to the owner; the scrypt parameters; macOS's device flush; a local POSIX filesystem; Detent's unchecked TLS keys sent to its ledger | Detent's table, D3, D4, Steps, Q9 |
| PKI-R13 | Q4's argument replaced; a CRL design's requirements recorded | D9, Q4 |
| PKI-R14 | Step 1's bound, 1,000 nodes per authority (an estimate); batch renewal; the behavior at 100,000 nodes | D2, D6 |
| PKI-R15 | The existing-tools option | Options |
| PKI-R16 | T1's serial; T2's dated leaves; T4's owner case and concurrency; T5's notAfter; T6's harness; T7's accept case; T9's OpenSSL per job; T10's SPKI bytes; rows for every missing case | Gate rows |
| PKI-R17 | `init`, `show`, `export`, `list` filters, `passphrase`, and `repair` specified; the docs page's contents; LibreSSL's limit; CA keys through `OSSL_STORE` | D3, D4, D12, D14 |
| PKI-R18 | The review tier and the done criteria; Detent's fixture described as it is; flag 1 corrected | Why, Steps, Flags |
| SR1 | No CN (as PKI-R5); reactor's own CN fallback put to the owner as an additive opt-in | D5, T1, Q8 |
| SR2 | The role in the path (as PKI-R1); a check that reads the URI requires the path's role and its direction's EKU | D5, T3 |
| SR3 | SANs kept; `sign` warns on a name another identity holds; the exposure in days, by case; an authority-wide life cap; a cron check on every node | D5, D6, D9, D11, Threat model, Q5 |
| SR4 | Flag 1 states `fsGroup`'s effect; the Kubernetes path follows SYS-13 | Flags, D12, T2 |
| SR5 | scrypt N = 2^14, r = 8, p = 1, and why; a 12-character minimum; the passphrase file checked; unprotected keys flagged; no core dumps; wiped buffers | D3, D4, T8 |
| SR6 | `--name` required; the request's SPKI printed on both hosts and checked; the request parsed before the passphrase, through a minimal surface | D6, T4 |
| SR7 | `DIR` opened once and checked; files reached through `openat`; exclusive random temp names; the lock before the index; a local filesystem | D3, T4 |
| SR8 | Backup; an authority compromise recovers on a fresh host; the new bundle replaces the old; kept identities' source named, and `--rekey`; an overlap warning; a planned rotation evicts nobody | D7, D8, D13, Threat model |
| SR9 | RSA only with `--allow-rsa`, which states the forward-secrecy cost | D4, Threat model |
| SR10 | The cluster fixed at `init`, without an override; a critical URI name constraint, optional DNS and IP ones; the bundle kept out of system stores | D5, D12 |
| SR11 | SPIFFE IDs: nothing to percent-encode or normalize; verifiers compare bytes; vectors | D5, T13, Q7 |
| SR12 | The SPKI digest over the SPKI DER; serial bit 6; T5 never asserts at notAfter | D5, D10, T1, T5, T10 |
| SR13 | Threat-model rows rescoped and added | Threat model |
| SYS-2 | Peers authenticate by SPKI pins in Track C's D16, extended; this design prints the pins and defines no peer check; Detent's switch stated as a proposal; the behavior at 100,000 nodes stated | Xapiand's needs, D2, D5, D6, D9, D10, Q3, Q5, Q6 |
| SYS-13 | Every key file pki reads passes at 0640 only with the caller's own group, and wider modes are refused; the Kubernetes path follows the same rule | D3, D12, Flags, T2, T4, T8 |
| SYS-15 | The private-file check written once, under SYS-13's rule, with completion's integrity rule as its second policy | D3, Q9 |

## Why

Both consumers verify certificates and neither makes them. Xapiand serves its REST API over
TLS from `--tls-cert` and `--tls-key` (`docs/design/transport-security.md:62-65`) and leaves
where they come from to the operator. Detent requires mutual TLS on its authority peer
transport and HTTPS for publishers (`Detent/docs/AUTHORITY_TRANSPORT.md:5`), pins each
voter's leaf (`:13`), and does not generate a CA. Today an operator hand-runs `openssl`, and
each project carries its own test script with its own profile: Xapiand's
`harness/tls_certs.sh:13-19` (P-256; a leaf with only a SAN and an EKU) and Detent's
`tests/detent_authority/tls_certificates.cmake:12-23` (RSA 2048; a leaf with
`basicConstraints`, `keyUsage` with `keyEncipherment`, both EKUs, `DNS:localhost`, and both
key ids, `:15`). Both are test fixtures; neither is a procedure an operator can follow,
rotate with, recover with, or audit.

The helper makes one authority per cluster, issues node certificates under one profile, and
rotates, backs up, and recovers that authority, offline, so an operator gets TLS without
learning `openssl x509 -req`.

## What each consumer needs

### Xapiand

| Need | Today | Source |
|---|---|---|
| A server certificate for the API, with DNS and IP SANs that clients verify | PEM chain and key; key mode 0600 or stricter, 0640 with the service's own group once the TLS gate lands SYS-13; valid now; a warning within 14 days of expiry | `transport-security.md:62-65, 107-113`; `src/tls.cc:30-32, 46-53` |
| A CA file for clients | `curl --cacert`, newman `--ssl-extra-ca-certs` | `transport-security.md:141` (T1) |
| Client certificates | None: no reactor role verifies a client optionally, so the API asks for none | `transport-security.md:64-65`; `reactor/reactor_tls.h:48-51` |
| Node certificates for mutual TLS | Deferred to strata's peer transport, which authenticates peers by SPKI pins (SYS-2) | `transport-security.md:125`; `strata/docs/PLAN.md:26-29` |
| Rotation | A restart per certificate change; nodes load no CA (`Options{cert, key, ""}` in the Server role) | `transport-security.md:115-116` (D1.7); `src/tls.cc:39, 42` |
| Revocation | None | |

Under strata, a member's transport identity (address and certificate pin) comes from a node
directory keyed by node id; pins rotate there without a `Configuration` entry and apply
without a restart (`strata/docs/design/track-c.md:212-215, 766-771`). The owner decided the
model (SYS-2): peers authenticate by public-key (SPKI) pins, defined in one peer transport
design, Track C's D16 extended, and every design that names peer identity defers to it,
transport-security's deferred D2 to D8 and this one included. This design supplies the
certificates and prints the SPKI pins; it defines no peer verification. Strata targets at
least 100,000 nodes (`strata/docs/PLAN.md:450`).

### Detent

| Need | Today | Source |
|---|---|---|
| A cluster CA | `peer_tls` and `publisher_tls` each name `certificate`, `key`, and `ca`; replicated configuration requires both | `AUTHORITY_TRANSPORT.md:47` |
| Peer certificates, both ends presenting one | reactor's `Peer` role: `verify_peer` and `verify_fail_if_no_peer_cert` | `reactor/reactor_tls.h:83-85`; `AUTHORITY_TRANSPORT.md:13` |
| Peer identity | A SHA-256 of each voter's leaf DER, exactly one per voter, unique, the local leaf matching its own pin; a constant-time scan of every pin | `consensus/include/consensus/transport/tls.h:12-20, 21-36, 45-66`; `AUTHORITY_TRANSPORT.md:13, 47` |
| Publisher, relay, and status certificates | A DNS name or IP SAN the client verifies through `bind_hostname` | `AUTHORITY_TRANSPORT.md:47`; `Detent/docs/RELAY.md:65`; `Detent/docs/LIBRARY_BOUNDARIES.md:250` |
| A CA file for agents | Certificate verification is required | `Detent/docs/USAGE.md:530` |
| Key files | The relay's credential files are regular files with one link, no symlinks, mode 0600 or 0640 for any group (`RELAY.md:15`; `detent_delivery/configuration.cc:64-68` refuses only `0027`). The TLS keys that `peer_tls` and `publisher_tls` name reach `reactor::tls::configure` unchecked (PKI-R12) | `Detent/src/detent_authority/service.cc:293-300`; `Detent/src/detentd/main.cc:257-258` |
| Rotation | A pin change on every voter's configuration, and a restart | `AUTHORITY_TRANSPORT.md:47`; `tls.h:24-35` |
| Revocation | Replacing a voter's pin, which re-keys that voter. A pin cannot be removed while its voter stays, and removing the voter is a membership change that durable recovery rejects (PKI-R2) | `tls.h:24-27`; `AUTHORITY_TRANSPORT.md:49` |
| Voters | At least 3, an odd number | `service.cc:308-310` |

Detent's maintainer proposals list generated certificates as a later item: a `reactor::pki`
with a first-start CA, signed node certificates, and single-use enrollment tokens, "Xapiand
qualifies it first; reviewed as security code before any adoption"
(`Detent/docs/DEPENDENCY_UPGRADES.md:79`).

### What reactor's roles already give

`reactor::tls::configure` (`reactor/reactor_tls.h:56-86`) sets TLS 1.2 or later, no
renegotiation, no 0-RTT, and a 64 KiB certificate-list cap for every role (`:64-69`). The
`Server` role presents a certificate and verifies none; `Peer` requires one from both ends;
`Client` verifies the server and may present one (`:48-51, 83-85`). The authority is one PEM
file loaded with `load_verify_file` (`:81`), which trusts every certificate in it: the
security review's compiled probe accepts leaves of both CAs of a two-CA bundle in the `Peer`
role and refuses each with a one-CA bundle (`sprobes/reactor_probe.cc`, cases 1a to 1c).
`bind_hostname` (`:91-101`) checks a DNS name or an IP literal and sets no host flags
(`:97-99`), so OpenSSL falls back to the subject's CN when a leaf has no DNS SAN (PKI-R5,
SR1). Nothing in reactor checks a URI SAN, loads a CRL, or swaps a context, and no consumer
reads a certificate's subject: no `X509_get_subject_name` or `NID_commonName` in reactor,
http, consensus, cluster, Xapiand's `src/` outside the Xapian fork, or Detent's `src/`.

The helper needs no reactor or consensus change to serve either consumer today.

## Options

### Form

| Option | What it is | For | Against |
|---|---|---|---|
| A. Library | OpenSSL 3 calls behind a small C++ API | Testable in CI on every platform; usable later by an in-cluster enrollment | An operator cannot run a library |
| B. A command in Xapiand | `xapiand pki ...` in the server binary | One binary already packaged for Docker, Homebrew, FreeBSD | Detent would depend on Xapiand, or copy it |
| C. A separate tool | A library and a command entry point built from it, in its own repository | Both consumers ship it; one profile; one test suite | A new repository to maintain (Q1) |
| D. Documentation and scripts | A guide and a shell script over `openssl` | No new code; `openssl` is everywhere | The profile lives in shell; no uniqueness, lock, or atomic write; the two existing scripts already disagree |
| E. An existing authority tool (PKI-R15) | Smallstep's `step` CLI, cfssl, or easy-rsa on the authority host; SPIRE where issuance is online | Maintained by others; SPIRE issues SPIFFE IDs (D5) online, at scales step 1 does not serve | Each adds a Go binary or a shell layer over `openssl` to the authority host, beside the OpenSSL 3 both consumers already link; D5's profile, D6's uniqueness and renewal rules, D3's data-root refusal, and the pins pki prints would be written as policy around it, which is D's weakness again |

The library is the mechanism and the command its operator surface, so A and C are one
choice. D is what exists today, twice. Past step 1's bound (D2), issuance moves online, into
the peer transport design SYS-2 names, or to a tool like those in E under D5's profile.

### Where the authority key lives

1. **Offline, on an operator host.** The key never reaches a serving node. Issuing needs the
   operator.
2. **In a node's data directory.** Every backup and volume copy carries it, and the process
   that parses every HTTP request holds it (TS-R8, `review:318-345`).
3. **On an online issuing intermediate**, with an offline root (`review:93-95`). Needs an
   enrollment listener and protocol (TS-R7), which neither consumer has.

### Bootstrap of a new node

1. **Manual copy.** The authority issues a key and a certificate; the operator copies them.
   The key travels.
2. **A certificate request.** The node makes its key and a request; the operator carries the
   request to the authority and the certificate back. The key never travels; neither file is
   secret.
3. **A join token and enrollment over the cluster.** Unattended, but it needs a listener
   that serves nodes without certificates, a token bound to the authority's key and the new
   node's name, an atomic persisted consume, and a request policy (TS-R7, `review:279-316`).
   Neither consumer has the listener; the peer transport design (SYS-2) is where it lands.

### Rotation without downtime

A leaf is renewed by issuing a new one and restarting the node (Xapiand's D1.7; Detent has
no reload either). An authority is rotated by trusting two CAs during an overlap: every
consumer's CA file holds the old and new certificates, then leaves move to the new CA, then
the old CA leaves the file. Cross-signing the new CA with the old avoids distributing the
bundle twice but puts two chains on the wire and makes path building depend on the
verifier; it is not chosen.

Detent's pins rotate apart from its CA. A renewed leaf has a new DER and so a new pin, and
each voter accepts exactly one pin per voter (`tls.h:24-31`). One key signed by two CAs
gives two DER digests and one SPKI digest (revision 0's probe, reproduced by both reviews).
The architect's model of consensus's pin rules (`probes/quorum_model.py`, a model of
`tls.h:21-36, 45-66`, not a run of Detent) gives, for one renewal with DER pins:

- With 3 voters, 2 of the 6 restart orders keep quorum: a peer first, then the renewed
  voter, then the other peer. The other 4 lose quorum during their second restart.
- With 5 voters, all 120 orders keep quorum.

For renewing every leaf, as a rotation does, 3 voters need at least 5 restarts with DER pins,
in D7's schedule; a second pin per voter needs the same 5; SPKI pins with same-key renewals,
SYS-2's model, need 3, in any order (`probes/quorum_model.out`, `trace_quorum.out`).

## Threat model

The design under attack: an offline authority (D2), requests instead of traveling keys
(D6), and D5's profile.

| Actor | Capability | Stopped by | Residual |
|---|---|---|---|
| Network, between a client and Xapiand's API, or between Detent's peers | Read, tamper, impersonate a node | TLS with chain and name verification; Detent's peers verify both ends and pin | Traffic analysis. Xapiand's node-to-node traffic stays in clear (`transport-security.md:32-34`; SR13). A client that skips verification gets confidentiality against a passive observer only (`:35-36`) |
| Holder of a valid `client` leaf (SR2) | Present it to a verifier that expects a node | Peers pin (Detent's DER pins today, SPKI pins under SYS-2), so a client leaf is no node's; its identity's path says `client`, and it carries `clientAuth` only; no verifier asks for a client leaf today | A later verifier of client certificates that checks neither the path nor the EKU |
| Holder of one node's key (SR1, SR3, SR6) | Impersonate that node, renewed leaves included, since every leaf is public | Its SANs limit it to that node's names for hostname-checking clients, with no CN to fall back to (D5); pins limit it to that node, Detent's today and SYS-2's later; `--name` and `--expect-spki` keep it from naming another node at signing (D6) | Every name it shares with other nodes, such as a load-balanced API name, extends it to the whole API (D6 warns). It lasts until notAfter, D8, or a pin change; under same-key renewal the leaf's life bounds only the tail after a re-key or a decommission (D9, Q5) |
| Reader of the authority host's files, no passphrase (SR5) | Every file in `DIR` | The CA key under scrypt N = 2^14, r = 8, p = 1, about 0.1 s a guess on one core (SR5's measurement); a 12-character minimum (D4) | Offline guessing at that cost, about 10^4 guesses a second on one GPU (SR5's estimate). The index (names, SANs, certificates) is readable |
| Holder of the authority: its key and passphrase, root on its host, or a backup with the passphrase (SR8, SR10) | Mint any identity and any DNS or IP name | The URI name constraint holds its identities to its trust domain (D5); pins, which no minted certificate satisfies without the pinned node's private key; recovery on a fresh host (D8); the bundle kept out of system stores (D12) | Every DNS and IP name, unless `init` added DNS and IP constraints, to every client trusting the bundle until it holds the new one |
| Writer of the authority directory (SR7) | Replace `trust.pem`, edit the index, plant a link at a temp name | `DIR` opened once and checked (owner, 0700); every file reached through `openat`; random, exclusive temp names; derived files rewritten from the index (D3) | The caller's account owns the authority |
| Writer of a consumer's certificate files (SR13) | On a node, replace its CA file so it trusts a rogue CA, or redirect `--tls-key` through the symlink `src/tls.cc:30-32` follows; on a client, replace `trust.pem` | File modes; pins hold against a rogue CA; Xapiand's step-1 nodes load no CA (`src/tls.cc:39`) | The account that writes those files owns that consumer's trust; Xapiand follows symlinks on purpose (flag 1) |
| During a planned rotation (SR8) | Two live CA keys on the authority host; a backup from then carries both | `list` warns past 30 days of overlap, and `backup` says it holds two keys (D7, D13) | The overlap lasts as long as the operator leaves it. A planned rotation evicts nobody (TS-R5), so compromise is always D8 |
| During an emergency (SR8) | Persist on the host whose compromise is the case; impersonate to clients still holding the old bundle | An authority compromise recovers on a fresh host and a fresh `DIR`; the new bundle replaces the old in every client, CI configuration, and trust store (D8) | Every client the operator does not reach keeps trusting the old CA |
| Reader of a node's backup or volume copy | Every file in its data directory | The authority never lives there: `init` refuses a directory at or under a Xapiand data root (D3) | A node key the operator stored in the data directory is that node's key (row 3) |
| Unprivileged user on a node | Read the node key | Xapiand refuses a key readable by group or others (`src/tls.cc:32`), and under SYS-13 accepts group read only for the service's own group; pki writes keys at 0600 (D3) | Members of the service's own group, under SYS-13. Detent checks none of its TLS key files (PKI-R12), so there the key's mode is the operator's |
| Recorder of traffic who later steals a key (SR9) | Decrypt recorded static-RSA sessions | ECDSA by default; RSA only with `--allow-rsa` (D4); Xapiand sets `strict` (`src/tls.cc:40`) | An RSA leaf on a context without `strict`, such as Detent's today (`DEPENDENCY_UPGRADES.md:77`) |
| Operator error | An expired leaf or authority; a duplicate identity; a wrong clock | Lifetime clamps, D6's uniqueness rules and clock refusal, `list --expiring` (D11) | An operator who never runs the check; Xapiand still warns at 14 days (`transport-security.md:112-113`) |

## Decisions

**D1. A library and its command entry point, in their own repository.** A small MIT library
over the OpenSSL 3 API (target `pki::pki`; OpenSSL 3.0 is the floor, as for `reactor::tls`)
whose target also holds the command's entry point, `pki::run(consumer, argc, argv)`. Each
consumer's binary calls it with its own consumer fixed (`xapiand pki ...`, Q2), so every
consumer runs the same code (PKI-R3; `KRONUZ_REFACTORING_DIRECTIVES.md` §9). The library's
`kronuz-pki` is its example and test driver, built only when the repository is top-level
(§9) and never packaged. Not `reactor::pki`: issuance is policy, and reactor stays a runtime
(TS-R17). The serving process never runs issuance code; a command invocation is its own
process.

Its dependencies reuse the catalog: `io` for EINTR-safe calls and `io::full_fsync`
(`F_FULLFSYNC` on macOS, `io/io.hh:406-424`), which Xapiand already builds
(`CMakeLists.txt:584-592`) and compiles into its own objects (`:1400`), so Xapiand links pki
against those; and `char-classify` for `chars::tolower`, the table Xapiand's node names
lowercase through (`src/node.h:141`, `strings/strings.hh:335-341`,
`char-classify/chars.hh:176-219`). No Kronuz library makes or signs certificates: on
2026-10-17, `gh repo list Kronuz --limit 1000` lists 257 repositories whose only crypto
entries are forks of JavaScript and Python projects (jsrsasign, pycrypto), and the
`cpp-libraries` index has no such entry. The library is crypto and secrets code; it lands
after a code review and a security review (Steps; PKI-R18).

**D2. Offline only in step 1, up to 1,000 nodes per authority.** The authority lives on an
operator host. Bootstrap is a certificate request (D6). There are no join tokens, no
enrollment listener, and no online intermediate in step 1. Enrollment returns with the peer
transport design that SYS-2 names (Q6), with TS-R5 to TS-R8 as its requirements; of TS-R8,
escrow and recovery land here (D8, D13; PKI-R7), and renewal retries with jitter wait for
automatic renewal.

The bound is an estimate (PKI-R14). At 365-day leaves, 1,000 nodes renew 2.7 times a day
(`probes/index_scale.out`), as same-key renewals that carry no out-of-band check and sign in
batches (D6); a rotation's renewals are one batch of 1,000. An index record is about 0.7 KiB,
mostly a P-256 leaf's 612 to 640 base64 characters (SR's leaves are 458 to 478 bytes of DER),
so the index holds about 6,000 records, 4 MiB, at five years, rewritten whole about three
times a day (D3).

*At 100,000 nodes* (SYS-2 asks for this statement), offline issuance does not hold:

- Renewals: 274 a day at 365-day leaves, 1,111 at 90 days (`probes/index_scale.out`), each a
  carried request, beside the simulator's nominal churn of 11 node replacements a day (as the
  system review reads it, SYS-2).
- The index: about 600,000 records at five years, so about 0.4 GiB rewritten on each of those
  operations, by the estimate above.
- A planned rotation: three rolling restarts of every node whose consumer loads the CA bundle,
  as Detent's peers do, at D7's steps 1, 3, and 4, since neither consumer reloads (Xapiand's
  D1.7); for Xapiand's step-1 API nodes, one rolling restart, with every client's bundle
  replaced twice.
- An emergency rotation: one restart of every node at once, since it has no overlap, and a
  consumer on the new bundle cannot verify a node still presenting an old leaf.

So 100,000 nodes need what step 1 lacks: enrollment, a reload, and pins that change without a
restart, all in the peer transport design (SYS-2, Track C's D16 extended). This design gives
that design D5's profile and D10's pins.

**D3. The authority directory.** Given by `--authority DIR`, with no default. Format 1:

| Path | Mode | Content |
|---|---|---|
| `DIR/` | 0700 | Owned by the caller |
| `lock` | 0600 | Held with `flock`, exclusively, from before the index is read until the command ends |
| `index` | 0600 | The authority's whole record, and the commit point of every operation |
| `ca/<g>/key-<k>.pem` | 0600 | CA generation g's PKCS#8 key, encrypted (D4) unless the authority was made with `--no-passphrase`; the index names the file |
| `ca/<g>/cert.pem` | 0644 | Its certificate, derived from the index |
| `trust.pem` | 0644 | The bundle every consumer loads as its CA file, derived from the index |

*The index* (PKI-R4). ASCII lines ending in LF, with fields separated by one space. Nothing
is escaped, because no field's alphabet holds a space, an LF, or a comma: the cluster is D5's
identity alphabet, times are digits, certificates are base64. Every fact a certificate
carries (serial, identity, role, SANs, validity, issuer, digests) is read from the
certificate, so no record repeats one.

```
index      = "pki-index 1" LF authority 1*ca *leaf *event end
authority  = "authority" SP consumer SP cluster SP copy SP max-days SP time LF
ca         = "ca" SP gen SP ca-state SP key-file SP protected SP der LF
leaf       = "leaf" SP leaf-state SP copy SP time SP checked SP der LF
event      = "event" SP time SP kind *(SP word) LF
end        = "end" SP 64(DIGIT / %x61-66) LF    ; SHA-256 of every byte before this line
consumer   = "xapiand" / "detent"
cluster    = 1*255(ALPHA / DIGIT / "." / "-" / "_")
copy       = number                            ; which copy of the authority (D13)
gen        = number                            ; a CA generation
max-days   = number
number     = %x31-39 *9DIGIT
ca-state   = "incoming" / "active" / "outgoing" / "retired"
leaf-state = "active" / "superseded" / "retired"
key-file   = "ca/" gen "/key-" number ".pem"
protected  = "yes" / "no"
checked    = "spki" / "index" / "local" / "recertified" / "imported"   ; how (D6)
time       = 14DIGIT "Z"                       ; UTC, YYYYMMDDHHMMSS
der        = 1*(ALPHA / DIGIT / "+" / "/") *2"="   ; base64 (RFC 4648 section 4) of a DER
kind       = word
word       = 1*64(ALPHA / DIGIT / "." / "-" / "_")
```

A reader refuses an unknown first line, any line outside the grammar, and a missing or
wrong `end`. It parses every certificate, and refuses one that is not the profile of its
format from a listed CA generation, two active leaves for one identity or one SPKI, more
than one `active` CA, and an `incoming` and an `outgoing` CA at once. Writers write exactly
this grammar. The test suite keeps a fixture per format version, starting with format 1, and
a later format adds its own (PKI-R4). The `end` digest catches corruption and truncation; it
is not keyed.

*Commit and recovery* (PKI-R6). Every command that changes the authority rewrites the index
once: a temp file in `DIR`, opened through `openat` with `O_CREAT | O_EXCL | O_NOFOLLOW`,
mode 0600, and a random suffix, is written, synced, and renamed over `index` with
`renameat`, then a directory barrier follows: `fsync` of `DIR`, and on macOS `F_FULLFSYNC`
of the open lock file, completion's technique
(`completion/include/completion/posix.h:443-453`). That rename is the operation. A new CA
key is written and synced under a fresh name before the rename that names it; derived files
and exports are written after. So the directory is always the old state, the new state, or
the new state with stale derived files. Every
command, under the lock, first removes temp files and the key files of generations the index
retired, and rewrites stale derived files. A key file the index does not name stops every
command until `pki repair` prints and removes it, since key material is never deleted
silently. An output lost to a crash is written again with `pki export` (D14). Every open of a
CA key checks that it matches its certificate in the index.

*Checks* (SR7, PKI-R12, SYS-13). `DIR` is opened once, with `O_DIRECTORY | O_NOFOLLOW`, and
refused unless `fstat` shows a directory owned by the caller with mode 0700, since whoever
writes it owns the authority; every file is reached through `openat` on that descriptor.
Every key file pki reads (the CA's, a node's under `request --key`, and a
`--passphrase-file`) is refused unless it is a regular file, not a symlink, with one link,
owned by the caller, with no mode bit outside 0640, and group-readable only when its
group is the caller's own. That is the owner's rule for key files (SYS-13), the one
transport-security's D1.6 takes on `tls/d1-gate`. "The service's own group" is read here as
the effective group (`getegid()`): a Kubernetes pod sets `fsGroup` to its `runAsGroup` (flag
1), and the TLS gate's check must read it the same way. A passphrase file inside `DIR` is
refused (SR5). pki writes its own keys at 0600.

These checks are one dependency-free header, about 60 lines of POSIX calls, written once
(SYS-15). It takes the policy as a parameter: SYS-13's rule for keys, which guards
confidentiality, and completion's rule for journal files, which refuses group- and
world-writable files (`mode & 0022`, `posix.h:105, 335`) and guards integrity. The other
implementations are Xapiand's (`src/tls.cc:30-32`), completion's (`posix.h:102-108,
330-337`), and four sites in Detent (`detent_cli/daemon.cc:58-61`,
`detent_relay/host.cc:213-216`, `detent_delivery/configuration.cc:64-68`,
`detent_authority/service.cc:203-206`); Q9 proposes one home for all of them.

The filesystem must be local and POSIX: `fstatfs` must report `MNT_LOCAL` on macOS and
FreeBSD, and a non-network type on Linux, since `flock` is not reliable across NFS clients
(SR7). FAT and exFAT keep no POSIX modes, so the mode check refuses them, naming the
filesystem.

*Placement.* `init` refuses a `DIR` that is, or is under, a directory holding
`.xapiand/node`, Xapiand's data-root marker (`src/manager.cc:292, 309`).

*Secrets* (SR5). The passphrase comes from the terminal or from `--passphrase-file`, never
from argv or the environment, and no secret is printed or logged
(`Detent/docs/ASYNC_AUTHORITY_ARCHITECTURE.md:69`). The process sets `RLIMIT_CORE` to 0, and
on Linux `PR_SET_DUMPABLE` to 0; it `mlock`s passphrase and key buffers where it can and
wipes them with `OPENSSL_cleanse`. CA keys are opened through `OSSL_STORE`, so a later format
can name a `pkcs11:` key in the CA record without changing anything else (PKI-R17).

**D4. Keys, signatures, and the passphrase.** The tool generates ECDSA P-256 keys and signs
with SHA-256; the CA is P-256. A request may carry a P-256 key, which every TLS 1.3
implementation verifies (RFC 8446 §9.1 requires `ecdsa_secp256r1_sha256`), a P-384 key, or,
only with `--allow-rsa`, an RSA key of 2,048 to 4,096 bits; anything else is refused
(PKI-R11, SR9). `--allow-rsa` exists for keys held in hardware that offers only RSA, and for
transport-security's static-RSA refusal case (D12). It prints that an RSA leaf gives no
forward secrecy on a context without `strict`: OpenSSL negotiates static RSA there even
when keyUsage lacks `keyEncipherment` (`probes/rsa_ku_static.out`; SR9's `reactor_probe`
case 4a). RSA leaves carry `digitalSignature` only, so a client that enforces keyUsage
refuses static RSA with them.

The CA key is PKCS#8, PBES2 with AES-256-CBC under scrypt N = 2^14, r = 8, p = 1 (16 MiB of
memory): the most OpenSSL's own PKCS#8 reader accepts, since its 32 MiB default limit
refuses N = 2^15 on write and on read (SR5's probe; the 4.0.3 CLI picks the same parameters,
`probes/pkcs8_scrypt.out`). `openssl pkcs8` from OpenSSL 3.6.5 and 4.0.3 reads it (the same
probe; T8 adds 3.0), so recovery needs no pki binary; LibreSSL, which is macOS's
`/usr/bin/openssl`, cannot (PKI-R17). At about 0.1 s a guess on one core (SR5), the
passphrase carries the strength: `init`, `rotate begin`, and `passphrase` refuse one under 12
characters and print how long one derivation took on this host. An authority made with
`--no-passphrase`, for test fixtures, records its keys as unprotected, and `list` and every
signing command say so.

**D5. The certificate profile.** The authority sets every field; a request supplies its
public key and proof of possession, and its attributes are never decoded (TS-R7's request
policy). The peer transport design takes this profile as a requirement (SYS-2).

| Field | Value |
|---|---|
| Version, serial | v3; 16 bytes from `RAND_bytes` with bit 7 of the first byte cleared and bit 6 set, so the DER content is always 16 bytes, 126 of its bits random (PKI-R16, SR12). With bit 7 cleared alone, 1 issuance in 256 encodes in 15 bytes (`probes/serial_length.out`) |
| Validity | notBefore is issuance less 3,600 s, for a node whose clock lags (TS-R7); notAfter is `--days` after issuance, by default and at most the authority's `--max-days` (365; Q5), and refused past the CA's notAfter |
| Subject | Empty (PKI-R5, SR1) |
| subjectAltName | Critical, as RFC 5280 §4.1.2.6 requires with an empty subject. Exactly one URI, the identity, as `X509-SVID.md` §2 requires; DNS and IP names from `--dns` and `--ip` |
| Leaf | `basicConstraints=critical,CA:FALSE`; `keyUsage=critical,digitalSignature`; subject and authority key ids |
| Roles | `server`: `extendedKeyUsage=serverAuth`, and at least one DNS or IP name (PKI-R5); `peer`: `serverAuth,clientAuth`; `client`: `clientAuth` |
| CA | Subject `CN=<consumer> CA <g> <16 hex digits>`: the consumer, the generation, and the first 64 bits of its SPKI digest, so no two CAs share a subject, which breaks path building for a leaf without an authority key id (PKI-R11; `probes/same_subject_bundle.out`); `basicConstraints=critical,CA:TRUE,pathlen:0`; `keyUsage=critical,keyCertSign,cRLSign`; `nameConstraints=critical,permitted;URI:<trust domain>`, with DNS and IP subtrees when `init` is given `--permit-dns` or `--permit-ip` (SR10); subject key id; notBefore issuance less 3,600 s; a default life of 1,826 days (5 years) |

*The identity* (PKI-R1, SR2, SR11) is a SPIFFE ID, as the `spiffe/spiffe` repository's
`standards/SPIFFE-ID.md` defines it, in a certificate shaped by its `standards/X509-SVID.md`:

```
spiffe://<trust domain>/<consumer>/<node | client>/<name>
```

- The trust domain is the cluster's name, lowercased; the consumer and the cluster are fixed
  at `init` (PKI-R8). Xapiand's default cluster, `Xapiand` (`config.h.in:281`), is
  `xapiand`.
- `node` is the path for `server` and `peer` leaves and `client` for `client` leaves. D6
  keys uniqueness on this path, so a client `n1` and a node `n1` are two identities.
- The name is Xapiand's node name, lowercased, as Xapiand compares names (`src/node.h:141`),
  or Detent's voter id in decimal, 1 to 2^64 - 1.
- Lowercasing is per byte and ASCII only, through `chars::tolower`, the table `strings::lower`
  uses. After it, the trust domain and the name must each be 1 or more of `[a-z0-9._-]`
  (`SPIFFE-ID.md` §2.1, §2.2), the name neither `.` nor `..`. SPIFFE refuses percent-encoding
  (§2.2), so there is no encoder, and one identity has one byte form. Every name Xapiand
  generates is ASCII letters (`src/xapiand_namegen.h:21-28`, over fantasyname's letter-only
  syllables, `fantasyname/c++/namegen.cc:31-193` at Xapiand's pin a87942a); an operator's
  `--name` outside the alphabet is refused at issuance, and the message gives the rule.
- The caps are each some mechanism's: the trust domain at 255 bytes (`SPIFFE-ID.md` §2.3); a
  Xapiand name at 511 bytes, the most Xapiand's node file round-trips (`char buf[512]`,
  `src/manager.cc:291-294`). An identity is then at most 791 bytes, under SPIFFE's 2,048
  (§2.3).
- Cluster names that differ only in ASCII case share a trust domain, a collision in
  `SPIFFE-ID.md` §2.1.1's sense: they take separate authorities, and no bundle holds both.
- A verifier builds the expected string and compares bytes; it never parses or normalizes
  the presented one (SR11).

A `peer` leaf is an X.509-SVID. `server` and `client` leaves narrow the EKU to one
direction, which `X509-SVID.md` §4.4 does not allow, so they carry a SPIFFE ID without being
SVIDs; neither consumer validates SVIDs, and both check the EKU of the direction through
OpenSSL (`probes/profile_check.out`).

*What reads it.* No verifier reads the URI, and peers need not: they authenticate by SPKI
pins (SYS-2), which D10 prints. The URI names the leaf for operators and for `list`, carries
the role, and is what a later check of the API's client certificates would read (TS-R9). Such
a check requires the path's role and the EKU of its direction (SR2), and under the URI name
constraint every OpenSSL verifier refuses an identity outside the CA's trust domain (SR10's
probe). No reactor role requests an optional client certificate yet (TS-R11).

*Why no CN* (PKI-R5, SR1). OpenSSL falls back to the CN when a leaf has no DNS SAN and the
check sets no host flags, as `bind_hostname` does not: a URI-only leaf with `CN=node1`
verifies for host `node1` under `openssl verify`, `s_client`, reactor's `Client` role, and
macOS curl (`probes/cn_fallback_tls.out`; SR1's `reactor_probe` case 3e). An empty subject
leaves nothing to fall back to (error 62), passes `-x509_strict`, and macOS curl accepts it
for its DNS SAN, on OpenSSL 3.6.5 and 4.0.3 (`probes/empty_subject.out`). The URI as the CN
also stops the fallback (SR1's `node2uri`), but OpenSSL refuses a commonName over 64
characters (`probes/cn_length.out`), which would cap identities far below their own caps.
RFC 9525 §1.3, which obsoletes RFC 6125, asks the same of both sides: neither include nor
check domain-like strings in the CN. Reactor's own fallback is Q8.

*Consequences fixed with the CA* (PKI-R11). `pathlen:0` rules out an online issuing
intermediate (Options, key location 3) under a generation; one arrives with a rotation (D7)
to a generation that allows it. A name constraint added later also needs a new generation,
which is why the URI constraint is in generation 1. In a CA's last 365 days a default `sign`
is refused, and the message names `rotate begin` and the largest `--days` that fits. The CA
subject leaves the cluster out because OpenSSL caps commonName at 64 characters and a trust
domain runs to 255; the name constraint binds the cluster and shows it.

**D6. Requests, signing, and uniqueness.** (PKI-R8, SR3, SR6, PKI-R10)

On the node, `pki request --cluster C --name N [--role R] [--key FILE] --out FILE` writes a
new P-256 key (0600, `O_EXCL`) unless `--key` names the one to keep, and a request (PKCS#10)
with an empty subject and no attributes. It prints the identity the node expects and the
key's SPKI digest (D10), for the operator to compare at signing.

On the authority, `pki sign --authority DIR --role R --name N [--dns D]... [--ip A]...
[--days D] [--expect-spki HEX] [--replace] --out FILE REQUEST` works in this order, touching
no secret before step 5:

1. The lock, `DIR`'s checks, and the index (D3).
2. The request: at most 16 KiB, read whole; its PEM armor decoded; then `d2i_X509_REQ`,
   `X509_REQ_verify` under its own key, `X509_REQ_get_pubkey`, and `EVP_PKEY_public_check`
   (SR6). Of the parsed request only its subject is read, and only to refuse a CN, which
   other tools write, that is not the name (PKI-R8); its attributes are never decoded. The
   key's type is D4's. The 16 KiB holds the largest accepted request, an RSA 4,096 key's,
   several times over.
3. The identity, from the authority's trust domain and consumer, the role, and `--name`
   (D5).
4. The rules below. Then the leaf to be issued is printed: identity, role, DNS and IP names,
   SPKI digest, validity, issuing generation, issuance time, and every warning.
5. The passphrase, the CA key (checked against its certificate), the signature, the index
   committed, and only then `--out`, created exclusively, never over an existing file.

| The index holds, for the identity | The request's key | Needs |
|---|---|---|
| No active leaf | Any key not active elsewhere: a first issuance | `--expect-spki` equal to the digest `request` printed on the node, carried apart from the request (SR6); recorded `checked spki` |
| An active leaf | The same key: a renewal | Nothing more, since the index vouches for the key (`checked index`). The leaf keeps the active leaf's DNS and IP names unless new ones are given, and a renewal may move a node between `server` and `peer` |
| An active leaf | Another key: a re-key or a takeover | `--replace` and `--expect-spki`; the old leaf becomes `superseded` |
| Anything | A key active under another identity | Refused: one key never names two identities, since a pin names one node (PKI-R2, SYS-2) |

- A DNS name is a lowercased LDH name of at most 253 characters, in labels of at most 63
  (RFC 1035 §2.3.4), with no `*`; an IP address is parsed and recorded canonically. A name
  already on another active leaf prints a warning listing its holders: a shared name, such as
  a load-balanced API name, extends a stolen key to the whole API (SR3).
- The clock (PKI-R10): `sign` refuses when the host's clock reads earlier than the newest
  time in the index, the authority's creation or its latest issuance, and prints the
  issuance time before the passphrase, so the operator compares it with a trusted clock.
  `--accept-clock` overrides a newest time that was itself wrong, and the index records the
  override.
- `sign --renew DIR --out OUTDIR` renews every request in `DIR` under one passphrase, each a
  same-key renewal of an active leaf whose identity the index supplies, since only the
  identity holding a key can renew with it. Any other request refuses the batch before the
  passphrase, unless `--skip-refused`. The batch is one index commit (PKI-R14).
- A renewal is a request made with `--key`, so the node keeps its key and its SPKI pin
  (SYS-2); a request with a new key is a re-key, which changes the pin and needs `--replace`.
- An expired leaf renews like any other: its record stays active until superseded or retired
  (TS-R8's re-enrollment of an expired node).
- `pki issue` makes the key and the leaf on the authority host, for test fixtures and
  development, and says that the key travels (`checked local`).
- `pki retire --role R --name N` frees an identity, for a decommissioned node; its leaf
  stays valid until notAfter (D9).

A node presents one leaf on every listener: a `peer` leaf with DNS names serves the API or a
publisher listener too.

**D7. Planned rotation.** Steps per consumer (PKI-R9):

| Step | Command | Xapiand in step 1 (nodes load no CA) | Detent (voters and agents load `ca`) |
|---|---|---|---|
| 1. Trust both | `rotate begin`: generation g+1 is `incoming`; `trust.pem` holds both | Clients get the two-CA bundle; no node restarts | Voters and agents get the two-CA bundle; voters restart one at a time, in any order, since every link stays up |
| 2. Switch | `rotate switch`: prints both CAs' DER digests, asks the operator to confirm that every consumer prints the same for its bundle (`pki fingerprint`), and records the confirmation; g+1 becomes `active` and signs, g becomes `outgoing` | Nothing changes | Nothing changes |
| 3. Renew | `sign --renew` (D6), from g+1 | Each node gets its new leaf and restarts, in any order | Each voter gets its new leaf. DER pins: the schedule below; SPKI pins: one restart each, in any order |
| 4. Retire the old | `rotate finish`: refused while an active leaf is from g, unless `--force`, which retires those leaves and prints them; g is `retired` and its key removed; `trust.pem` holds g+1 | Clients get the one-CA bundle; no node restarts | Voters and agents get the one-CA bundle; voters restart one at a time, in any order |

Detent with 3 voters and DER pins, after all three new leaves are issued, so every new pin is
known (PKI-R2; `probes/trace_quorum.out`):

| Restart | Voter | Presents | Its pins change to |
|---|---|---|---|
| 1 | 0 | Its old leaf | Voter 2's new pin |
| 2 | 2 | Its new leaf | Its own and voter 1's new pins |
| 3 | 1 | Its new leaf | Every new pin |
| 4 | 0 | Its new leaf | Every new pin |
| 5 | 2 | Its new leaf, unchanged | Voter 0's new pin |

Each step keeps a voter linked to a majority. One renewal alone is three restarts: a peer,
installing the new pin; the renewed voter, presenting its leaf; then the other peer.

- `rotate abort`, before `switch`, retires g+1 and removes its key. After `switch`, it also
  retires every leaf g+1 issued and prints them, and g becomes `active` again; those nodes
  renew from g before their clients drop g+1. Either way the one-CA bundle goes out again.
- The overlap runs from `begin` to `finish`, with two live CA keys on the authority host;
  `list` warns past 30 days of it (SR8). Begin with the CA a year from its notAfter; `list`
  warns from then.
- A planned rotation evicts nobody, since a compromised node renews like the rest (TS-R5).
  Compromise is D8.

**D8. Emergency rotation and the lost authority.** (TS-R5, PKI-R4, PKI-R7, SR8)

- *A node key is compromised, and the authority is not.* `pki rotate emergency --keep FILE
  --out OUTDIR` in the same `DIR`: generation g+1 is `active` at once, with no overlap;
  every other generation is `retired` and its key removed; every leaf whose identity `FILE`
  does not list is `retired`; and each listed identity is recertified, as a new leaf under
  g+1 for the public key and names in its stored certificate, so no request travels and SPKI
  pins hold. With `--rekey`, none is issued, and each kept identity sends a fresh request,
  signed as a first issuance (D6). `trust.pem` holds g+1 alone.
- *The authority or its host is compromised.* A fresh host and a fresh `DIR`: `pki init`,
  then each kept identity either with `--rekey` or recertified with `pki recertify CERT...`
  from the certificate its node presents, each checked against that node's SPKI digest,
  never from the old host's index. The old key is destroyed wherever it is reachable.
- *The authority is lost*, to a disk or a forgotten passphrase: restore the newest backup
  (D13); without one, take the compromised path above, recertifying from the nodes'
  certificates.
- *Distribution*, in every case: every consumer gets the new bundle, and its leaf, in one
  restart, and a node not kept cannot connect. The new bundle replaces the old in every
  client, CI configuration, and trust store. Appending it, as a planned rotation trains
  operators to do, keeps the old CA trusted (SR8).

**D9. No revocation in step 1.** (PKI-R13, SR3) Neither consumer checks a CRL or OCSP, and
reactor loads none (`reactor_tls.h:56-86`). No step-1 verifier would load one: Xapiand's
nodes verify no client (`src/tls.cc:39`), Detent's peers pin, and API clients would each have
to opt in. An offline authority must also re-sign a CRL before every nextUpdate, or every
verifier fails closed: error 12, "CRL has expired", an hour after nextUpdate in the
architect's probe (`probes/crl_expiry.out`). A CRL would take effect at each verifier's next
restart, as `trust.pem` does. For peers, revocation is replacing a pin: under SYS-2 a pin in
the node directory changes without a restart (`track-c.md:766-771`).

The exposure, with 365-day leaves: a stolen key's leaf stays valid until notAfter, up to 365
days, renewals included while the thief goes undetected; a re-keyed or decommissioned
node's old leaf, for the rest of its life; a pinned peer's, until its pin is replaced. D8
ends any of them, at the cost of a cluster- and client-wide event (SR3). A CRL design
carries: the nextUpdate re-signing cost; a `reactor::tls::Options` field added last and off
by default, since consumers initialize `Options` positionally (`src/tls.cc:39`, Detent's
`service.cc:299`, reactor's `test/tls_test.cc:136-137`; PKI-R3); and Xapiand's deferred
reload (TS-R12).

**D10. Fingerprints.** `pki fingerprint FILE` prints, for each certificate in it, the SHA-256
of its SubjectPublicKeyInfo DER, `i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x))`, which is the pin
of SYS-2's model, and the SHA-256 of its DER, which is Detent's voter pin today
(`AUTHORITY_TRANSPORT.md:47`; consensus's `certificate_fingerprint`, `tls.h:12-20`).
`X509_pubkey_digest` hashes only the key's bits and gives another value
(`probes/spki_digest.out`; SR12), so the peer transport design hashes the same SPKI DER, a
requirement this design hands it. Both print as 64 lowercase hex digits, a form Detent's
parser takes (`service.cc:320-335`). `request`, `show`, and `sign` print the SPKI digest;
`sign` and `rotate` print DER digests as well, so an operator checks what reaches each
consumer.

**D11. Expiry visibility.** `pki list --expiring [T]` prints the active leaves and CAs past
five sixths of their life, as TS-R8 asks (61 days before notAfter for a 365-day leaf), or
within T when given, and exits nonzero when there are any. `pki check FILE [--within T]` does
the same for one certificate, for a cron job on each node, which the docs recommend whatever
the leaf's life (SR3). `list` also warns for a CA within a year of its notAfter, an overlap
past 30 days (D7), and an unprotected CA key (D4). The consumers keep their own checks
(`src/tls.cc:46-53`; transport-security's daily check, `:112-113`).

**D12. Step 2: Xapiand adopts it.**

- The harness: `harness/tls_certs.sh` becomes `pki init --no-passphrase` and `pki issue`,
  writing the same files (`ca.pem`, `server.pem`, `server.key`, and `ca.key`), so
  `tls_check.sh` and `e2e_capture.sh` keep their interfaces. `tls_check.sh`'s RSA leaf
  becomes `pki issue --allow-rsa`, which keeps the static-RSA case honest: keyUsage refuses
  nothing there, so the refusal still comes from `strict` (`probes/rsa_ku_static.out`). Its
  dated leaves (expired, not yet valid, and on `tls/d1-gate` one expiring in 3 days) stay
  `openssl ca`-made (`harness/tls_check.sh:33-59`), signed with the fixture CA's key, which
  `tls_certs.sh` copies to `ca.key` from the `--no-passphrase` authority's `ca/1/key-1.pem`;
  pki itself signs only the dates D5 allows (PKI-R16).
- Transport-security's T1 and T2 rerun on these certificates, SYS-13's key cases included
  (T2 here).
- The command ships in the Docker image, the Homebrew formula, and the FreeBSD package (Q2).
- A TLS operations page in the user docs, with no request blocks for the docs E2E suite to
  run. It covers issuance; renewal and the cron check; rotation per consumer; emergency,
  backup, restore, and the lost authority; certificates brought from elsewhere under D5's
  profile, SPIRE's included (TS-R2, `review:159`); shared names (SR3); `trust.pem` in
  `--cacert`-style settings only, never in a system store or a process-wide setting such as
  `NODE_EXTRA_CA_CERTS` (SR10); a non-root Kubernetes pod under SYS-13 (flag 1); and
  LibreSSL's limit (D4).
- A changelog bullet under `[Unreleased]`.
- Detent's adoption is its maintainer's (Q3).

**D13. Backup and restore.** (PKI-R7, SR8)

- `pki backup --authority DIR --to BACKUP` creates `BACKUP`, which must not exist, at mode
  0700, and copies the index and every live CA generation's key and certificate through D3's
  write path, then reads the copy back and compares digests. The keys stay encrypted, and
  the passphrase is stored elsewhere. The copy's index ends with a `backup` event, and every
  command that changes an authority refuses one, so a backup never issues. During an overlap
  it says that the copy holds two CA keys. `pki list --authority BACKUP` reads a backup
  without the passphrase.
- `pki restore --from BACKUP --authority NEWDIR` creates `NEWDIR`, raises the copy
  generation above every one the backup has recorded, records the restore as an event, and
  opens each CA key to prove the passphrase. When `BACKUP` is writable it records the restore
  there too, so a second restore of it is refused without `--again`. It prints the copy
  generation, the backup's newest record time, and that the copy it replaces must never issue
  again.
- Uniqueness after a restore covers what the backup held. `pki import CERT...` adds leaves
  issued since the backup, each verified against one of the authority's CA generations; one
  whose identity or SPKI is active in another record is a conflict, printed and refused.
- The copy generation is provenance: every command prints it, and every record names the
  copy that wrote it. The tool cannot see two copies at once, so the procedure destroys or
  seals the copy a restore replaces.

**D14. Commands.** (PKI-R17) The operator surface, all through D1's entry point:

| Command | Does |
|---|---|
| `init --authority DIR --cluster C [--max-days N] [--ca-days N] [--permit-dns S]... [--permit-ip N]... [--passphrase-file F \| --no-passphrase]` | Makes generation 1, the authority record, and `trust.pem`; asks for the passphrase twice. The consumer is the calling binary's (D1); `kronuz-pki` takes `--consumer` |
| `request`, `sign`, `sign --renew`, `issue`, `retire` | D6 |
| `show FILE` | A request or a certificate: identity, role, names, key type, SPKI and DER digests, validity, issuer |
| `list [--issuer G] [--state S] [--role R] [--expiring [T]]` | The index's records, filtered, with D11's warnings |
| `export SERIAL --out FILE` | A leaf from the index, again |
| `fingerprint FILE`, `check FILE [--within T]` | D10, D11 |
| `rotate begin`, `rotate switch`, `rotate abort`, `rotate finish [--force]`, `rotate emergency --keep FILE [--rekey]` | D7, D8 |
| `recertify CERT...`, `import CERT...` | D8, D13 |
| `backup`, `restore` | D13 |
| `passphrase` | Re-encrypts every live CA key under a new passphrase, as new files that one index commit names |
| `repair` | Prints and removes key files the index does not name (D3) |

## Where the reviews differ

What this revision does where the architect's review and the security review disagree:

1. **The CN.** SR1 allows the URI or nothing; the architect finds the URI capped at 64
   characters. Decided: nothing (D5), the form both accept.
2. **Reactor's CN fallback.** SR1 sets `X509_CHECK_FLAG_NEVER_CHECK_SUBJECT` in
   `bind_hostname`; the architect calls that an interface change for every consumer, to be
   opt-in or coordinated. pki's leaves need neither, since they have no CN. Recommended: an
   additive opt-in. The owner's, as Q8; it changes reactor's interface.
3. **The leaf's life.** SR3 recommends 90 days, or at least a cap; the architect recommends
   365. Recommended: 365, with SR3's cap as the authority's `--max-days` and the exposure in
   days (D9). The owner's, as Q5.
4. **The serial's short-encoding rate.** SR12 says 1 in 128; the architect, 1 in 256. It is
   1 in 256: DER keeps a zero first byte when the next byte's top bit is set
   (`probes/serial_length.out`). The fix, bit 6 set, is the same (D5).
5. **A `sign --cluster` override.** SR10 allows one with a warning; the architect allows
   none. Decided: none, since under the URI name constraint every OpenSSL verifier refuses an
   overridden leaf anyway (SR10's own probe).
6. **Xapiand's key rule.** SR4 asks for a controlled 0640 or a documented non-root path, the
   architect finds the 0640 rule sound, and TS-R23 set 0600. The owner decided (SYS-13):
   0640 only with the service's own group, any wider mode refused. The TLS gate changes
   `src/tls.cc` and D1.6, and pki's checks apply the same rule (D3).
7. **The identity's form.** SR11 specifies a percent-encoder; the architect offers SPIFFE
   as the alternative and leaves the choice to the owner. Recommended: SPIFFE (D5), the
   owner's as Q7.
8. **A strata peer's identity.** The architect puts it to the owner (PKI-R1); the security
   review favors SAN identity for Xapiand and SPKI pins for Detent. The owner decided
   (SYS-2): SPKI pins, in one peer transport design.

## Flags from the reading

1. **Key files under Kubernetes** (SR4, PKI-R18). A Secret mounted with `defaultMode: 0600`
   passes `src/tls.cc:32` only in a pod running as root. With `fsGroup` set, the kubelet adds
   group read to every file of the volume, so 0600 becomes 0640 and 0400 becomes 0440, with
   group `fsGroup`; a non-root pod, as under the restricted Pod Security profile, reads a
   Secret only through those group bits or world bits, all of which `src/tls.cc:32` refuses
   today (SR4, from the kubelet's `pkg/volume/volume_linux.go` and
   `pkg/volume/secret/secret.go`). The documented StatefulSet sets no `securityContext`
   (`docs/src/content/docs/reference-guide/kubernetes.md:46-76`), so it runs as root, reads
   the key before `demote()` (`src/main.cc:1097, 1105`), and works. Under SYS-13 a non-root
   pod works too, once the TLS gate lands it: `runAsGroup` and `fsGroup` set to one group,
   and the Secret at `defaultMode: 0400` or `0600`, so the key is 0440 or 0640 with the
   service's own group. Step 2's docs page gives that recipe. Following symlinks stays, since
   Secret volumes mount through `..data` links; pki's refusal of symlinks applies to the files
   pki itself reads.
2. **The trust bundle in reactor.** `load_verify_file` trusts every certificate in the file,
   and the security review's compiled probe shows two anchors work in the `Peer` role
   (`sprobes/reactor_probe.cc`, cases 1a to 1c). No reactor test covers it; T3 adds one in
   pki's suite, changing no reactor file (PKI-R3).
3. **Detent's proposal names `reactor::pki`** (`DEPENDENCY_UPGRADES.md:79`), and TS-R17 put
   issuance outside reactor. That row is Detent's maintainer's to update, through Detent's
   agent.
4. **SYS-13 says "as Detent does"**, and Detent accepts group read for any group: its
   credential check refuses only `0027` (`detent_delivery/configuration.cc:64-68`), its relay
   docs say "controlled group-readable `0640`" (`RELAY.md:15`), and its TLS keys are not
   checked at all (PKI-R12). SYS-13's own-group rule is stricter than any of them; the item
   for Detent's ledger carries the difference.

## Gate rows

A new `harness/pki_check.sh` in Xapiand runs the rows marked "harness"; the library's tests
run the rest. Every refusal sits beside an accepted case that differs in one variable, and
asserts the reason the tool prints. Test builds add two hooks that release builds never
contain: `PKI_TEST_KILL_AT`, a kill between two writes, and `PKI_TEST_NOW`, the clock.

| Row | Target | Procedure |
|---|---|---|
| T1 Profile | Every role's leaf passes `openssl verify -x509_strict -purpose sslserver` (server, peer) and `-purpose sslclient` (peer, client) against `trust.pem`; `openssl x509 -noout -text` shows exactly D5's fields, an empty subject and a critical SAN holding one URI among them; across 1,000 issuances every serial's DER content is 16 bytes with a first byte from 0x40 to 0x7f; notBefore is issuance less 3,600 s, within 5 s; a request asking for `CA:TRUE`, an extra SAN, `keyUsage=keyCertSign`, and a subject yields none of them; a leaf without a DNS SAN is refused for a hostname equal to its node name (`-verify_hostname`, error 62), beside the same leaf accepted for an IP it holds; a leaf the fixture CA's key signs for another trust domain, forged with `openssl x509 -req`, is refused under the name constraint, beside one forged for its own; the CA's subject, `pathlen:0`, and constraints are D5's | Library test; harness `profile`, under OpenSSL 3 and 4 |
| T2 Xapiand handshakes | Transport-security's T1 and T2 pass on pki certificates, with the same assertion counts as on `tls_certs.sh`'s, T2's SYS-13 cases included (a key at 0640 or 0440 with the service's own group starts the node; with another group, or at 0644 or 0660, startup is refused, naming the reason); `curl --cacert trust.pem` verifies `localhost` and `127.0.0.1` and refuses a name not in the SANs; under the URI-constrained CA, curl on Linux and on macOS, newman, and Python's `ssl` each verify a leaf, and if one refuses, the constraint becomes opt-in at `init` before step 2 ships | `harness/e2e_capture.sh` with `CA=trust.pem` (`:26-53`); `harness/tls_check.sh` (D12); harness `xapiand` |
| T3 Peer handshakes | reactor's `Peer` role at both ends with `peer` leaves: accepted; a `server` leaf presented as a client, and a `client` leaf as a server: refused (error 26); a `client` leaf's identity is under `/client/`, never `/node/` (SR2); a leaf of another authority: refused; a two-CA `trust.pem` accepts leaves of both CAs, and each one-CA bundle refuses the other's (flag 2); consensus's `authenticated_voter` returns the configured voter for the DER pin pki printed, and refuses a renewed leaf until the pin changes | Library test over memory BIO pairs, in the pattern of `reactor/test/tls_test.cc:76-108`, linking `reactor::tls` and `consensus::transport` |
| T4 Issuance refusals | Each refused with its reason, beside an accept that fixes its one variable: a first issuance without `--expect-spki`, and with another key's (a substituted request, SR6); another key for an active identity without `--replace`; a key active under another identity; a request whose CN names another node; a `server` leaf with no DNS or IP name; `--days` above `--max-days`, and past the CA's notAfter, naming `rotate begin`; a `*`, a non-LDH name, or a 254-character DNS name; a name outside D5's alphabet, or of 512 bytes; a bad request signature; a request over 16 KiB; RSA without `--allow-rsa` (RSA 2,048 accepted with it); RSA 1,024 with it; P-521 (P-384 accepted); a CA key that is a symlink or has two links; a CA key, node key, or passphrase file at 0640 with a group other than the caller's own, at 0644, or at 0660, each beside the same file at 0640 with the caller's own group, accepted (SYS-13); a key owned by another user, run as root against a fixture owned by `nobody`, since a non-root caller fails the open first (a CI step under `sudo` on Linux and macOS); `DIR` group-writable, at 0750, or another user's (SR7); a passphrase file inside `DIR`; `DIR` under a `.xapiand/node` root. Concurrency, in processes, through the lock: eight `sign`s of eight identities give eight distinct serials and eight records; eight of one identity give one leaf and seven refusals | Library test; harness `refusals` |
| T5 Expiry and clocks | A leaf verifies at `-attime` issuance less 3,599 s and is refused at less 3,601 s; it verifies at notAfter less 1 s and is refused at notAfter plus 1 s, on OpenSSL 3 and 4 alike, and no case sits at notAfter itself, where 3.6.5 refuses and 4.0.3 accepts (`probes/attime_bounds.out`); `list --expiring 30d` lists exactly the leaves within 30 days and exits 1, and exits 0 with none, and without a duration lists those past five sixths of their life; `sign` with the clock before the index's newest time is refused naming both times, and passes with `--accept-clock`, which the index records | `openssl verify -attime`; library test with `PKI_TEST_NOW`; harness `expiry` |
| T6 Planned rotation | A 3-node Xapiand cluster serving HTTPS, and a client requesting each node every 100 ms with `--cacert trust.pem`, through `begin`, `switch`, the renewals, and `finish`: zero verification failures; nodes restart only for their renewals; the client's bundle changes at `begin` and at `finish`; refused connections during each restart counted and stated. After `finish`, an old-CA leaf is refused, and a client holding only the old CA refuses a new leaf. `abort` before `switch`, and after it with the incoming CA's leaves retired and printed; `finish` refused with an old-CA leaf active, and `--force` retiring and printing it. D7's Detent schedule keeps a voter linked to a majority at every step, and the 4 bad orders of one renewal lose it | Harness `rotation` over `harness/cluster_check.sh`, which curls plain HTTP today (`:107, 136, 153, 162`) and gains `SCHEME`, `CA`, and per-node `--tls-cert` and `--tls-key` in `launch_node` (`:90-103`); the Detent schedule as a library test over consensus's `validate_peer_identity` and `authenticated_voter`, with memory BIO pairs |
| T7 Emergency rotation | After `rotate emergency --keep FILE`, `trust.pem` holds one CA; every old leaf is refused by `openssl verify` and by curl; each kept identity's new leaf verifies against the new bundle and keeps its SPKI; the active records are exactly the kept identities. With `--rekey`, no leaf is issued, and a kept identity's fresh request signs as a first issuance | Harness `emergency`; library test |
| T8 Files and secrets | Under umask 000 and 077 alike, D3's modes. With `PKI_TEST_KILL_AT` at every write of every operation, the directory is the old state, the new state, or the new state with stale derived files that the next command completes, and never a CA key that mismatches its certificate, a leaf the index lacks, or two active leaves for one identity. The CA key is PKCS#8 under scrypt N = 2^14, r = 8, p = 1, read back by `openssl pkcs8` from OpenSSL 3.0 and from 4.0; a wrong passphrase is refused, and so is one under 12 characters at `init`. The passphrase and key bytes appear in no stdout, stderr, or argv, and an abort mid-sign leaves no core file. An index with a wrong `end`, an unknown version, or a line outside the grammar is refused, and format 1's fixture reads | Library test; harness `files` |
| T9 Platforms | The library's tests pass on Linux x86_64 and arm64 against OpenSSL 3.0 (Ubuntu 24.04's packages), on macOS against Homebrew's `openssl@3` (3.6) and `openssl@4` (4.0), and on FreeBSD 14 against its base OpenSSL 3.0; and under ASan, UBSan with `-fno-sanitize-recover=all`, and TSan. T4's concurrent signs are processes, which TSan does not see; TSan covers the code inside one process | The library's CI |
| T10 Fingerprints | `fingerprint`'s SPKI digest equals `openssl x509 -pubkey -noout \| openssl pkey -pubin -outform DER \| openssl dgst -sha256`, and differs from `X509_pubkey_digest`'s; its DER digest equals `openssl x509 -outform DER \| openssl dgst -sha256` and consensus's `certificate_fingerprint`; both are 64 lowercase hex digits that Detent's parser takes; `request`, `show`, and `sign` print the same SPKI digest; a same-key renewal keeps it and changes the DER digest | Library test; harness `fingerprints` |
| T11 Backup and restore | `backup`, then `restore` on a fresh path: `list` shows the same records, with the copy generation raised; a leaf signed from the restored copy verifies; a node whose leaf expired while the authority was away renews from it by its SPKI; a second `restore` of the backup is refused without `--again`; a changing command on the backup itself is refused; a leaf issued after the backup, once `import`ed, makes its identity need `--replace` again, and a conflicting `import` is refused; `list --authority BACKUP` reads without the passphrase | Library test; harness `backup` |
| T12 Lost authority | A 3-node Xapiand cluster over HTTPS loses `DIR`. On a fresh path, `init` a new authority, `recertify` each node from the certificate it presents, and distribute the new bundle and leaves, one restart each: every node verifies against the new bundle, an old leaf is refused, and every node keeps its SPKI | Harness `lost` |
| T13 Identity vectors | Each beside its accept: the name `Gryzhak` gives `.../node/gryzhak`, and the cluster `Xapiand` the trust domain `xapiand`; refused, an empty name, `.`, `..`, a `/`, a `%`, a space, a byte at or above 0x80, a 512-byte name, and a 256-byte trust domain; Detent's voter id 0 refused, and 1 and 2^64 - 1 accepted; a verifier's byte comparison refuses `node1` presented as `%6Eode1` or `NODE1` | Library test |
| T14 Fuzzing | The request intake, the index reader, and the certificate intake (`show`, `import`, `recertify`): one CPU-hour each at the gate's commit, zero findings, with a hang timeout; each corpus replayed as tests in CI | libFuzzer targets in the library, run by the test engineer under a host hold |

## Steps

1. **The library's skeleton**, where Q1 puts it: the `pki::pki` target and its entry point
   (D1); CI on T9's matrix; key generation (D4); fingerprints (D10; T10); the expiry checks
   (D11; T5's verification cases); the private-file header and `DIR`'s checks (D3; T8's mode
   cases and T4's file cases). Nothing here waits on Q7.
2. **The index, the profile, and signing**: D3's format and commit (T8); D5 (T1, T13), whose
   identity code waits on Q7; D6 (T3, T4); the fuzz targets (T14).
3. **Rotation and recovery**: D7, D8, and D13 (T6's library cases, T7, T11).
4. **Xapiand, step 2**: `xapiand pki` (Q2), packaging, the harness (D12; T2, T6, T7, T12),
   the docs page, and the changelog bullet. It starts from master after `tls/d1-gate` lands,
   since that branch edits `src/tls.cc`, `harness/tls_certs.sh`, `tls_check.sh`, and
   `e2e_capture.sh` (system review, SYS-13).
5. **Detent**: its maintainer's decision (Q3), through its agent.

The library is crypto and secrets code, and its index and profile are formats, so it lands
after its full design review clears, a code review, and a security review, as the pipeline's
tiers require (PKI-R18). The lead writes the done criteria into the ledger before step 1
starts: for steps 1 to 3, T1, T3 to T5, T8 to T11, T13, and T14, with ASan, UBSan, and TSan
clean and CI green on Linux, macOS, and FreeBSD.

### Cross-repository changes and their owners

(PKI-R3.) Neither consumer needs any of these for step 1. **Interface** marks a change to
reactor or consensus, which Detent links: such a change is additive, off by default, and
coordinated with Detent's maintainer.

| Change | Repository | Owner | Interface | When |
|---|---|---|---|---|
| The library and its entry point | `Kronuz/pki` (Q1) | The owner | New | Step 1 |
| Its entry in the index of libraries | `Kronuz/cpp-libraries` | The owner | | When it lands |
| `xapiand pki`, the harness, packaging, the docs page | Xapiand | This workstream | Xapiand's command line | Step 2 |
| The two-anchor bundle test | pki's suite, against `reactor::tls` | This workstream | None; no reactor file changes | Step 1 |
| SYS-13's key rule in `src/tls.cc` and transport-security's D1.6 | Xapiand, `tls/d1-gate` | The TLS gate's workstream (its ledger, 93bc2448e) | Xapiand's startup checks | Before the TLS gate report |
| SPKI pins beside certificate pins (SYS-2) | consensus | Detent's maintainer, proposed through Detent's agent | **Interface**: additive if certificate pins stay the default (Q3) | A Detent adoption pass |
| The peer transport design: identity by SPKI pins, the node directory, enrollment (SYS-2) | strata, Track C's D16 extended | The lead assigns it | Its own | Before C4's D16 code |
| An opt-in that stops `bind_hostname` falling back to the CN | reactor | The owner, with Detent's maintainer | **Interface**, additive (Q8) | When the owner decides |
| A CRL path in `reactor::tls::Options`, last and off by default | reactor | The owner, with Detent's maintainer | **Interface**, additive (Q4, D9) | With a CRL design |
| One home for the private-file check (SYS-15) | `io`, proposed | The owner | `io`'s, additive (Q9) | When the owner decides |
| Items for Detent's ledger: its TLS key files are unchecked (PKI-R12), and SYS-13's own-group rule is stricter than its relay's (flag 4); the `reactor::pki` row is this library, offline and without enrollment tokens (flag 3); SYS-2's SPKI pins, with D7's restart order until then (Q3) | Detent's `docs/DEPENDENCY_UPGRADES.md` | Detent's maintainer; the lead sends them through Detent's agent | | Now |

## Open questions for the owner

Each question carries this design's recommendation and both reviewers', side by side, and
the owner's decision where one already binds it. **Interface** marks an option that changes
reactor's or consensus's interface. Revision 0's Q1 to Q6 keep their numbers; SYS-2 and
SYS-13 settled the two others this revision would have asked.

**Q1. Where does it live?**

1. A new public repository, `Kronuz/pki`: MIT, the library and its entry point, under the
   deliverables standard (`KRONUZ_REFACTORING_DIRECTIVES.md` §6).
2. Inside reactor, as `reactor::pki`, as Detent's proposal says.
3. Inside Xapiand only (`src/pki/`), with Detent copying it or going without.
4. An existing tool instead (Options, E).

- Design: 1. TS-R17 rules out 2; 3 puts the profile where Detent cannot reuse it; 4 leaves
  D6's rules to scripts.
- Architect: 1, with the existing-tools row beside it (PKI-R15).
- Security: no recommendation; D1 ruled sound.

**Q2. What is the command called, and who ships it?**

1. A standalone `kronuz-pki`, packaged by each consumer.
2. A subcommand of each consumer's binary (`xapiand pki ...`) calling the library's entry
   point, with `kronuz-pki` built only in the library's CI, as its test driver and example,
   and never packaged.
3. Both, packaged.

- Design: 2. One binary is already packaged on three platforms, the subcommand fixes the
  consumer, and it runs as its own process (D1). A bare `pki` would collide with Dogtag's
  `pki` command, which Fedora's `dogtag-pki-tools` and Ubuntu's `pki-tools` packages ship.
- Architect: 2, with the entry point in the library target; otherwise 3.
- Security: no recommendation.

**Q3. Does Detent adopt it, and when does consensus switch to SPKI pins?** SYS-2 makes SPKI
pins the model and leaves consensus's switch from certificate pins to Detent's maintainer,
so this is a proposal for that maintainer, sent through Detent's agent.

1. Adopt pki's files now, with today's DER pins: a renewal is a pin change on every voter,
   three restarts in D7's order for one renewal and five for a rotation's renewals. No code
   changes.
2. Switch consensus to SPKI pins first, then adopt: a renewal is one restart, and a rotation's
   renewals three, in any order. **Interface**: an SPKI digest beside
   `certificate_fingerprint` (`tls.h:12-20`) as a per-configuration choice, certificate pins
   staying the default, and a `public_key_sha256` beside Detent's `certificate_sha256`
   (`service.cc:313-335`). D6's one-key-per-identity rule and D10's SPKI DER digest already
   serve it.
3. Neither for now.

- Design: 1 now, and 2 proposed on Detent's schedule as SYS-2's direction.
- Architect: 1 now, with D7's order written down, and SPKI pins proposed in Detent's ledger.
- Security: SPKI pins keep everything a DER pin gives, since a CA key holder can mint a
  certificate with a voter's public key but cannot finish the handshake without the private
  key, and they remove the renewal churn, provided both sides hash the same bytes (SR12) and
  the index refuses a shared SPKI.
- Owner (SYS-2): SPKI pins, with Detent's maintainer agreeing to the switch.

**Q4. Revocation.**

1. None in step 1 (D9).
2. A CRL: `pki revoke`, a `crl.pem` re-signed before every nextUpdate, and a reactor option
   that loads it with `X509_V_FLAG_CRL_CHECK`. **Interface**: a new `Options` field, last,
   off by default.

- Design: 1, for D9's reasons; for peers, revocation is replacing a pin (SYS-2).
- Architect: 1, with the reasons corrected (PKI-R13).
- Security: no CRL asked for; the exposure stated in days (SR3), which D9 now does.

**Q5. The default leaf life.**

1. 365 days.
2. 90 days.

- Design: 1, with an authority-wide maximum (`--max-days`, default 365), the exposure stated
  in days (D9), and a cron check on every node (D11); 90 days or less once enrollment
  renews and contexts reload. Under same-key renewal a leaf is public, so a thief who holds a
  node's key presents each renewed leaf just as the node does: the life bounds the tail after
  a re-key or a decommission, while each renewal costs a carried request, a restart (D1.7),
  and for Detent today an ordered pin change. A shorter life would bound theft only if every
  renewal took a new key, and under SYS-2 a new key is a pin change in the node directory:
  274 a day at 100,000 nodes on 365-day leaves, 1,111 on 90-day ones (system review, SYS-2).
- Architect: 1, for the same reasons (its "The security review", point 4).
- Security: 2, with `pki check --within` in cron even while renewal is manual, or at least a
  `--days` cap (SR3).

**Q6. Online enrollment.**

1. Defer it to the peer transport design that SYS-2 names, Track C's D16 extended, which the
   system review proposes should carry enrollment.
2. Design it now, against Xapiand's current remote and replication listeners.

- Design: 1. Those listeners are raw-fd paths that strata replaces (TS-R1), and an
  enrollment listener needs a role reactor does not have (TS-R7).
- Architect: 1; TS-R1 and TS-R7 still hold.
- Security: no recommendation; D2 ruled sound.

**Q7. The identity's form.** (PKI-R1, SR11)

1. SPIFFE IDs, `spiffe://<cluster>/<consumer>/<node | client>/<name>` (D5). Nothing is
   encoded, so one identity has one byte form, under a published grammar; `peer` leaves are
   X.509-SVIDs. Names outside `[a-z0-9._-]` after lowercasing are refused, which Xapiand's
   generated names never are.
2. A Kronuz scheme, `xapiand://<cluster>/node/<name>`, that allows every name: per-byte
   ASCII lowercasing, then every byte outside RFC 3986's unreserved set as `%XX` in
   uppercase hex (§2.1), with vectors; every verifier implements the same encoder, and the
   index stores encoded forms.

- Design: 1. Under SYS-2 the URI identifies no peer, so its value is a form that tools and
  people read one way; SPIFFE gives that without an encoder.
- Architect: either, with vectors; the owner chooses (PKI-R1).
- Security: SR11 specifies option 2's encoder. Under option 1 nothing is encoded, and T13's
  vectors hold the validator to the same standard; the re-check rules on whether that meets
  SR11.

**Q8. Should `reactor::tls::bind_hostname` stop falling back to the CN?** (SR1;
**Interface**)

1. Always: `X509_CHECK_FLAG_NEVER_CHECK_SUBJECT` set in `bind_hostname`, with a case in
   reactor's tests. A server certificate that names its host only in the CN then stops
   verifying for Detent's publisher and status clients and every other caller.
2. An opt-in, additive, with the default unchanged; Detent adopts it after its TLS tests, as
   with `strict`.
3. Neither: pki's empty subjects already leave nothing to fall back to.

- Design: 2. RFC 9525 §1.3 tells clients not to check the CN, and 1 would change every
  consumer's checks at once.
- Architect: an interface change, opt-in or coordinated with Detent.
- Security: 1.

**Q9. One home for the private-file check?** (PKI-R12, SYS-15; not blocking)

1. `io` gains it, and `openat`, additively, with both policies (D3). Xapiand already builds
   `io`, which has no dependencies and CI on every platform. Xapiand's check
   (`src/tls.cc:30-32`, which follows symlinks, as flag 1 needs), completion's, at its
   maintainer's choice since Detent links completion, and Detent's four sites move to it.
2. pki's header stays the only shared copy, and the others stay as they are.

- Design: 1.
- Architect: write it once, as a dependency-free header, and propose a shared home
  (PKI-R12).
- Security: no recommendation; SR7 asks for the checks and names no home.
- System review: one implementation, extracted once under SYS-13's rule (SYS-15).
