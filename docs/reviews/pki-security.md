# Xapiand security review: PKI helper design

- Date: 2026-10-17.
- Reviewed: `docs/design/pki.md`, revision 0 at 58aa0c7ee on `design/pki`, with
  `docs/status/pki.md`. The architect reviews the design as a whole in parallel; this
  review covers what an attacker can do under it. No code exists.
- Surfaces read: `src/tls.{h,cc}` and `harness/tls_certs.sh` at 58aa0c7ee;
  `reactor/reactor_tls.h` at e19afd9; `consensus/include/consensus/transport/tls.h` at
  fc66c0e; Detent 8412624, read-only: `docs/AUTHORITY_TRANSPORT.md`,
  `docs/PERSISTENT_TRANSPORT_ARCHITECTURE.md`, `docs/RELAY.md:15`,
  `docs/ASYNC_AUTHORITY_ARCHITECTURE.md:69`, `docs/DEPENDENCY_UPGRADES.md:57,59`,
  `tests/detent_authority/tls_certificates.cmake`; `strata/docs/design/track-c.md:211-215`;
  `strings/strings.hh:336-341`; the kubelet's `pkg/volume/volume_linux.go` and
  `pkg/volume/secret/secret.go` at `master`.
- Checked by: probes under `.scratch/review-pki-security/`, each with its output kept
  beside it. `profile.sh` issues D5's profile with the `openssl` CLI and runs the
  verification cases under OpenSSL 3.6.5 (`out-3.6.5/`) and 4.0.3 (`out-4.0.3/`).
  `reactor_probe.cc` runs handshakes over memory BIO pairs through
  `reactor::tls::configure` and `bind_hostname`, compiled against both versions
  (`reactor_probe-3.6.5`, `reactor_probe-4.0.3`). `scrypt/` measures OpenSSL's PKCS#8
  scrypt parameters. No network listener was started.
- Citations: `design:N` is line N of `docs/design/pki.md`; `review:N` is line N of
  `docs/reviews/transport-security-design.md`; `tls.h` is consensus's unless prefixed.

## Verdict

**APPROVE WITH CONDITIONS. Nothing blocks the design as a shape: an offline authority,
requests instead of travelling keys, authority-set extensions, random serials, a
backdated notBefore, lifetime clamps, and a uniqueness index are the right decisions, and
the probes confirm the profile verifies as D5 and D7 say under OpenSSL 3.6.5 and 4.0.3
and through reactor's roles. Two profile details are wrong in a way that no later code
can fix once a 365-day leaf exists (SR1, SR2), and the threat model leaves the attacker it
names most, a holder of one node's key, with more than it says (SR3). Those, with the
Kubernetes claim (SR4), the passphrase (SR5), request binding (SR6), and the authority's
loss or compromise (SR8), are conditions on revision 1, before step 1's code.**

Where this review departs from the transport-security review: TS-R2's "no CN fallback"
(`review:154-158`) is necessary and not sufficient, since OpenSSL falls back to the CN on
its own when a leaf has no DNS SAN (SR1); TS-R23's key-file rule (`review:816`) is right
on a host and blocks every non-root Kubernetes pod (SR4), where Detent's controlled 0640
(`RELAY.md:15`) is the better rule; and the review's answer on leaf pins (`review:89-92`)
holds for Xapiand, while for Detent an SPKI pin (Q3.3) keeps everything a DER pin gives
and removes the renewal churn, provided both sides hash the same bytes (SR12).

## Attackers considered

| Attacker | Under the design | Gains | Findings |
|---|---|---|---|
| On the network between nodes | Xapiand: nothing changes; node-to-node traffic stays in clear (`transport-security.md:32-34`). Detent: mutual TLS and pins | Everything between Xapiand nodes, as today; nothing on the API or Detent's peers | SR13 |
| On the network between a client and the API | TLS, chain and hostname verification | Nothing, unless the client skips verification | — |
| A client holding a valid `client` leaf | No verifier asks for one today | Nothing today; a node identity under any verifier that reads the URI without the EKU (strata's check, a future `--tls-ca`) | SR2 |
| One compromised node, holding its key | Valid until notAfter or D8 | Its own identity for the leaf's life; every hostname in its SANs, which includes a shared API name; the hostname equal to its CN; a request for any unissued name | SR1, SR3, SR6 |
| Reader of the authority host's files, no passphrase | `ca.key` is AES-256 under scrypt | An offline guess at the passphrase at about 0.1 s per guess; the index (names, clusters, digests) | SR5 |
| The same, with the passphrase; or root; or a stolen backup with it | Nothing | The authority: any identity, any cluster the CA served, until every consumer and client holds a new bundle | SR5, SR8, SR10 |
| Writer of the authority directory | Directory mode 0700, written not checked | Replaces `trust.pem`, edits the index, plants a symlink for a predictable temp name | SR7 |
| Writer of a node's certificate directory | Not in the threat model | Makes that node trust a rogue CA (Xapiand clients and the future URI check; Detent's pins hold); redirects `--tls-key` through the symlink `stat` follows | SR13 |
| During a planned rotation | Two CAs trusted; two keys live on the authority host; no expiry on the overlap | A backup or theft from the overlap yields two CA keys; the overlap is as long as the operator leaves it | SR8 |
| During an emergency rotation | A new CA on the same host; clients updated by hand | Persistence if the host is the compromised one; impersonation to every client still holding the old bundle | SR8 |
| A stolen backup | Of a node: whatever the operator put in the data directory. Of the authority: the encrypted key | A node key stored beside the data; the CA, if the passphrase is weak or stored with it | SR5, SR8 |
| A recorder of traffic who later steals a key | RSA leaves on non-strict contexts negotiate static RSA | Decryption of recorded sessions of that node | SR9 |

## Rulings

| Decision | Ruling | Findings |
|---|---|---|
| D1 Library and command, own repository | Sound: issuance out of the serving process | — |
| D2 Offline only | Sound; the authority's loss and compromise need a procedure | SR8 |
| D3 Authority directory | Sound in intent; the passphrase, the directory's own checks, and temp files are underspecified | SR5, SR7 |
| D4 Keys | Sound; RSA requests carry a forward-secrecy cost on non-strict consumers | SR9 |
| D5 Profile | Wrong in two fields: the CN is a hostname for URI-only leaves; the URI has no role | SR1, SR2, SR10, SR11, SR12 |
| D6 Issuance and uniqueness | Sound mechanism; the name comes from the requester and nothing binds it | SR6 |
| D7 Planned rotation | Sound; holds in reactor with a two-anchor bundle (probe 1a-1c); the overlap is unbounded | SR8 |
| D8 Emergency rotation | Right answer to TS-R5; runs on the host whose compromise is the case | SR8 |
| D9 No revocation | The stated exposure is understated by shared names and the 365-day default | SR3 |
| D10 Fingerprints | Sound; the SPKI digest must name its bytes | SR12 |
| D11 Expiry | Sound | — |
| D12 Harness adoption | Sound | — |

Detent's whole-certificate pin during renewal: no security loss, only availability. A
renewed leaf has a new DER digest and the same SPKI digest (probe: `node1.pem` DER
`309bccb1…`, `node1r.pem` DER `9c9218b3…`, SPKI equal, both versions), so every peer
refuses the renewed voter until its pin changes (`tls.h:63-64`), and the old leaf stays
accepted by every peer not yet restarted, which is the same node. An SPKI pin keeps the
property the design values, "a trusted CA alone cannot give an arbitrary certificate
holder a voter identity" (`tls.h:45-46`): a CA key holder can mint a certificate carrying a
voter's public key and cannot finish the handshake without the private key. Two
conditions make it safe: both sides hash the SubjectPublicKeyInfo DER (SR12), and the index
refuses an SPKI already active under another identity, so one key never names two voters
(`validate_peer_identity` refuses duplicate pins at configuration, `tls.h:28-31`, which is
the backstop).

## Findings

### SR1. The CN is a hostname for every leaf without a DNS SAN (medium)

Attacker: a holder of one node's key, or whoever is issued a `peer` or `client` leaf, or
a `server` leaf issued with `--ip` only; with one CA serving several clusters, a node of
one cluster named like a host of another.

Evidence:

- `design:195`: "Subject: `CN=<name>`, informational only". `design:193-194`: `peer` and
  `client` leaves carry the URI alone; `--dns` is optional for every role.
- OpenSSL's hostname check uses the CN when the certificate has no dNSName SAN unless
  `X509_CHECK_FLAG_NEVER_CHECK_SUBJECT` is set; reactor's `bind_hostname` sets no host
  flags (`reactor/reactor_tls.h:97-98`).
- Probe `profile.sh`: `openssl verify -CAfile ca.pem -verify_hostname node2 node2.pem`
  (URI-only SAN, `CN=node2`) prints `OK` under 3.6.5 and 4.0.3; `node2uri.pem` (CN set to
  the URI) is refused; `nodeip.pem` (a `server` leaf with `IP:127.0.0.1` and no DNS SAN,
  `CN=nodeip`) matches hostname `nodeip` under both.
- Probe `reactor_probe` 3e: a Client-role context with `bind_hostname("node2")` completes
  the handshake with the URI-only leaf (`client_verify=0`); 3f, CN set to the URI, is
  refused (62, hostname mismatch); 3d shows the CN ignored once a DNS SAN exists.
- Name constraints do not close it: under a CA with `permitted;DNS:.example`, `nccn.pem`
  (`CN=x`, URI-only) verifies on both versions, and `-verify_hostname x` prints `OK` on
  3.6.5 and exits 1 with no message on 4.0.3. The behavior is version-dependent.

What they gain: a node named `n` holds a certificate that every hostname-verifying client
trusting the bundle accepts for `https://n/`. Row 2 of the threat model, "names in the SAN
limit it to that node's names" (`design:133`), is false for leaves issued without `--dns`.

Fix: the CN is the identity URI, or absent; `cn2dnsid` never treats a string with `:` or
`/` as a DNS name (probe 3f, `node2uri`). `reactor::tls::bind_hostname` sets
`X509_CHECK_FLAG_NEVER_CHECK_SUBJECT` through `X509_VERIFY_PARAM_set_hostflags`, with a
case in `reactor/test/tls_test.cc`. T1 adds: a hostname equal to the CN is refused on a
leaf without a DNS SAN.

### SR2. The identity URI carries no role, so a client certificate names a node (medium)

Attacker: a holder of a valid `client` leaf, against any verifier that maps the URI to a
node without also requiring the EKU of the direction it expects: the reactor check strata
adds (`design:69-71,197-199`), or a future `--tls-ca` on the API (TS-R9).

Evidence:

- `design:192-193`: every role's URI is `<scheme>://<cluster>/node/<name>`; the role lives
  in the EKU only.
- Probe `reactor_probe` 2b: a `client` leaf (EKU `clientAuth`, URI
  `xapiand://c1/node/node3`) dials a Peer-role server and the handshake completes with
  both verify results 0, under both versions. The Peer role checks the EKU for the TLS
  direction only (2c: a `server` leaf as a client is refused, 26; 2d: a `client` leaf as a
  server is refused, 26), and a client-auth leaf is a correct TLS client.
- Consensus refuses it only through the pin (`tls.h:63-64`); a CA-only verifier has
  nothing left but the URI.
- The index keys uniqueness by identity (`design:206-207`), so a `client` named `n` and a
  node `n` collide, and the second needs `--replace`, which marks the node's line
  `replaced`.

What they gain: a node identity for the price of a client certificate, for the life of the
leaf. The profile is fixed at issuance; leaves issued under revision 0 carry it for 365
days.

Fix: the role in the path: `/node/<name>` for `server` and `peer`, `/client/<name>` for
`client`, which is the shape TS-R9 asked for (`review:381-382`). The identity check
requires the URI prefix of the role it expects and the EKU of its direction (`peer`:
`serverAuth` and `clientAuth`). T3 adds: a `client` leaf's URI is not under `/node/`.

### SR3. Shared names, no revocation, and 365-day leaves turn one stolen node key into API-wide impersonation for up to a year (medium)

Attacker: a holder of one node's key with a position between clients and the API, or
control of a DNS answer.

Evidence:

- `design:133`: a stolen node key is limited "to that node's names". `design:194`: `--dns`
  is free-form per leaf; the index records no SAN (`design:205-206`). A load-balanced API
  puts one name on every node's leaf: the Kubernetes guide fronts three replicas with one
  Service (`docs/src/content/docs/reference-guide/kubernetes.md:33-76`).
- D9 (`design:229-232`): valid until notAfter, D8, or a Detent pin change. Q5 defaults
  the leaf to 365 days (`design:189,335-340`). D8 is every node restarted plus every
  client's bundle replaced (`design:224-227`), and the client half has no inventory
  (`design:134`).

What they gain: impersonation of the API's shared name to every client that trusts the
bundle, for the rest of the leaf's life: up to 365 days, or until the operator runs a
cluster-wide emergency rotation and reaches every client.

Fix: the index records DNS and IP SANs; `sign` warns when a SAN is already on another
active identity and the docs say that a shared name widens row 2 to the whole API. State
Q5's cost plainly: 365 days of exposure per stolen key against 90; recommend 90 with
`pki check --within` in cron even while renewal is manual, or at least a `--days` cap in
the tool. The threat table says that without revocation the remedy for one key is a
cluster-wide and client-wide event.

### SR4. The Kubernetes claim fails under `fsGroup`: a non-root pod cannot satisfy `src/tls.cc:32` from a Secret (medium)

Attacker: none directly. The risk is the downgrade an operator takes when the documented
path refuses to start: running the pod as root, patching the check, or mounting the key
with world bits.

Evidence:

- `design:250-255`: "need `defaultMode: 0600`, since the default 0644 is refused, so the
  server-side check should stay".
- The kubelet marks secret volumes `ReadOnly: true` (`pkg/volume/secret/secret.go`,
  `GetAttributes`), and with `fsGroup` set, `changeFilePermission` runs
  `os.Chmod(filename, info.Mode()|mask)` with `mask = roMask = 0440` on every file
  (`pkg/volume/volume_linux.go`). `defaultMode: 0600` becomes 0640, group `fsGroup`.
- `src/tls.cc:32` refuses any group or other bit. Secret files are root-owned; a pod
  under `runAsNonRoot` (Pod Security "restricted") reads them only through `fsGroup`
  (0640 or 0440) or world bits (0644 or 0444). Every one is refused.
- The documented StatefulSet sets no `securityContext` (`kubernetes.md:46-76`), so the
  container starts as root, reads the key before `demote()` (`src/tls.h:16-17`), and
  works. A hardened cluster cannot use it.

What they gain: a root-running API process, or a check removed by hand, in the main
deployment path.

Fix: Xapiand accepts 0640 when the file's group is the process's effective group or
`fsGroup`, and keeps refusing world bits, which is Detent's "controlled 0640"
(`RELAY.md:15`); or the design's flag 1 says that a non-root pod copies the key to a 0600
`emptyDir` in an init container. Either way, flag 1 states the `fsGroup` behavior, and T2
adds a 0640 case beside the 0600 accept.

### SR5. The passphrase is the whole defense of a stolen authority copy, and the design does not bound it (medium)

Attacker: a reader of the authority host's files without the passphrase; a stolen backup
of `DIR`; a local user who obtains a core dump or a swap page of the tool.

Evidence:

- `design:160`: AES-256 under scrypt, no parameters. `design:167-168`: the passphrase
  comes from the terminal or a file. `--no-passphrase` exists for fixtures and nothing
  marks a key written with it. Nothing on core dumps, swap, zeroization, the passphrase
  file's own mode and links, or where it may live.
- Probe `scrypt/`: OpenSSL's PKCS#8 PBES2 path writes and reads scrypt at N=16,384, r=8,
  p=1 (16 MiB) and refuses N=32,768 with "memory limit exceeded", the provider's 32 MiB
  default, on write; the reader applies the same limit. The standard path therefore caps
  the cost at about 0.1 s per guess on one core (0.15 s wall measured, process start
  included). LibreSSL 3.3.6, macOS's `/usr/bin/openssl`, cannot read the file at all.
- At that cost a GPU makes on the order of 10^4 guesses per second. An 8-character
  lowercase passphrase (2^37.6) falls in about 7 months on one card; four diceware words
  (2^51.7) take about 10^4 card-years.

What they gain: with a weak passphrase, the CA key from any copy of `ca.key`.

Fix: state the parameters (N=2^14, r=8, p=1, and why: OpenSSL's reader limit); `init`
and `rotate begin` refuse a passphrase under 12 characters and print the per-guess cost;
`--passphrase-file` passes the same refusals as `ca.key` and is refused inside `DIR`;
`pki list` and `sign` say when `ca.key` is unencrypted; the tool sets `RLIMIT_CORE` to 0,
wipes passphrase and key buffers with `OPENSSL_cleanse`, and `mlock`s them best-effort. T8
adds: an abort mid-sign under the test hook leaves no core file.

### SR6. Nothing binds a request to the node that made it; the operator's eyes are the enrollment protocol (medium)

Attacker: anyone on the path the operator uses to carry `node.csr` and `node.pem`: a
shared drive, chat, email, a jump host; or a compromised node that requests another name.

Evidence:

- `design:201-204`: `pki request --name N` writes `node.csr`; `pki sign --role R
  --cluster C node.csr` takes no name, so the identity comes from the request, that is,
  from the requester. `design:183-184` says a request "supplies only its public key and
  proof of possession"; it also supplies the name.
- `sign` prints the leaf's digests (`design:235-237`); `request` prints nothing to compare
  them against.
- The uniqueness index (`design:207`) stops only names already active.

What they gain: swap the request's key in transit and the operator signs
`xapiand://c/node/N` for the attacker's key. The real node's key then mismatches its
certificate (`reactor/reactor_tls.h:79`), the operator sees a startup failure and reissues,
and the attacker's leaf lives 365 days (SR3). A compromised node requests any name not yet
issued.

Fix: `pki sign` requires `--name` and prints the identity and the request's SPKI SHA-256
before the passphrase prompt; `pki request` prints the same digest on the node and the
procedure compares the two, with `sign --spki <digest>` refused on mismatch. The request
is the one untrusted input the authority host parses: decode it before the passphrase
prompt, cap it at 16 KiB, and touch only `d2i_X509_REQ`, `X509_REQ_verify`,
`X509_REQ_get_pubkey`, and `EVP_PKEY_public_check`. T4 adds a substituted request.

### SR7. The authority directory's own trust is written, not checked; temp files and the lock are underspecified (low)

Attacker: an unprivileged user on the authority host with write access to `DIR` or its
parent (a group-writable home, a shared operator account), or one who races the tool.

Evidence: `design:157-159,169-174`: the refusals cover `ca.key` and `next.key`; the
directory is 0700 as written and absent from the refusal list; "a new file in `DIR`,
`fsync`, `rename`" names no `O_EXCL`, no unique temp name, and no `openat` on a verified
directory descriptor; `lock` is `flock`, which is advisory and unreliable on NFS; D3 holds
the lock "for any write", while the uniqueness check (`design:207`) is a read that must
sit under it. `trust.pem` and the index are protected by the directory alone
(`design:138`).

What they gain: a replaced `trust.pem`; an index line marked `retired` to get a duplicate
identity issued; a symlink at a predictable temp name as the rename's target. Modest,
since the design already counts the directory's writer as the trust owner.

Fix: open `DIR` with `O_DIRECTORY|O_NOFOLLOW` and `fstat` it: owned by the caller, mode
0700, else refuse; reach every file through `openat`; create temp files with
`O_CREAT|O_EXCL|O_NOFOLLOW`, the final mode, and a random suffix; take `lock` before
reading the index; say that `flock` needs a local filesystem. T4 adds a group-writable
`DIR`, refused.

### SR8. Loss or compromise of the authority host has no procedure (medium)

Attacker: a holder of the authority host (TS-R8's actor); a thief of a backup made during
an overlap; and, as a non-attacker, a failed disk.

Evidence:

- No backup, escrow, or restore anywhere in D3 or D8; TS-R8 required "an operator-run
  escrow export and restore" (`review:339-345`), and the options stop at "offline, on an
  operator host" (`design:89-90`).
- D8 (`design:224-227`) runs `rotate emergency` in the same `DIR` on the same host, which
  is the compromised host in the case it exists for.
- D7's overlap has no expiry (`design:221`), so `ca.key` and `next.key` are live together
  for as long as the operator leaves it, and a backup from then carries both.
- D8 issues "new leaves only for the identities listed" and names no source for them:
  under D6 the authority holds no node keys, so either it stores requests, and a re-sign
  reuses every kept node's key, or every kept node makes a new request, and every SPKI and
  every Detent pin changes.
- The planned rotation's procedure trains operators to append CAs to `trust.pem`; the
  emergency needs the opposite, and a client that appends keeps trusting the old CA.

What they gain: an attacker on the authority host survives an emergency rotation run
there; a backup thief gets two CA keys; an operator who loses the disk learns that the
only recovery is D8 across every node and every client.

Fix: a backup line in D3: the encrypted `ca.key` and the index, the passphrase stored
elsewhere, the copy verified by `pki list` from it. D8 says: on a fresh host, in a fresh
`DIR`, the old key destroyed where reachable, the old bundle removed from every client,
CI configuration, and trust store rather than appended; name the request source for kept
identities and a `--rekey` that demands fresh requests. `pki list` warns when an overlap
exceeds 30 days. Say that a planned rotation evicts nobody (TS-R5), so a compromise is
always D8.

### SR9. RSA leaves keep static-RSA key exchange on non-strict contexts (low)

Attacker: a passive recorder who later obtains the node's key.

Evidence: D4 accepts RSA keys of 2,048 to 4,096 bits (`design:180`); the leaf's
keyUsage is `digitalSignature` only (`design:191`). Probe `reactor_probe` 4a: a Server-role
context without `strict` and the RSA leaf negotiates `TLSv1.2 AES128-GCM-SHA256`, RSA key
exchange with no forward secrecy, under 3.6.5 and 4.0.3 alike; OpenSSL does not enforce
`keyEncipherment` for RSA key exchange. Xapiand sets `strict` (`src/tls.cc:40`); consensus
and Detent do not yet (`DEPENDENCY_UPGRADES.md:57`).

What they gain: decryption of recorded sessions of an RSA-leaf node after a key theft.

Fix: `sign` warns on an RSA request or needs `--allow-rsa`; D4 states that RSA leaves give
no forward secrecy on non-strict contexts; Detent's adoption of `strict`, already proposed,
closes it there.

### SR10. One CA, any name, any cluster: no name constraints and no cluster binding (low)

Attacker: a holder of the authority key; an operator host whose one authority serves
several clusters; a client that loads `trust.pem` into a general store
(`NODE_EXTRA_CA_CERTS`, a system store).

Evidence: D5 sets no `nameConstraints` (`design:188-195`); `--cluster` is an argument of
`sign` (`design:202`), so one `ca.key` issues for every cluster and the CA certificate
binds nothing. Probe `profile.sh`: a CA with
`nameConstraints=critical,permitted;URI:c1,permitted;DNS:.example,permitted;IP:127.0.0.0/255.0.0.0`
accepts a `c1` leaf and refuses a `c2` leaf under both versions; the CN bypass of SR1
remains.

What they gain: a stolen CA key mints any DNS or IP name a trusting client accepts and any
cluster's node identity; an emergency rotation then hits every cluster the CA served. A
constraint binds the certificate in the bundle, not the key: a stolen key cannot mint a
new unconstrained anchor that consumers trust.

Fix: `init --cluster C` records the cluster and `sign` takes it from the authority unless
`--cluster` overrides with a warning; optional `--permit-dns` and `--permit-ip` at `init`
add critical name constraints, with a constraint on `URI:<cluster>`, together with SR1's
CN fix; the docs say the bundle belongs in `--cacert`-style pinning and never in a system
store.

### SR11. The identity URI's canonical form is underspecified (low)

Attacker: a node whose name differs from another's only in case or encoding, against a
verifier built from a different encoder; or a verifier that normalizes the presented URI.

Evidence: `design:193`: "lowercased and percent-encoded outside RFC 3986's unreserved
set", "exact byte comparison". Unstated: the order (lowercase before encoding, or `%2F`
becomes `%2f`); the hex case of escapes (RFC 3986 §2.1 normalizes to uppercase); whether
"lowercased" is per byte, as Xapiand's `strings::lower` is (`strings/strings.hh:336-341`,
used by `src/node.h:139-142`), or Unicode-aware; that the cluster is the URI's host
(reg-name; `:` and `@` must be encoded); and that the verifier builds the expected string
and compares bytes rather than parsing the SAN.

What they gain: little today; a verifier that normalizes the presented value accepts
`xapiand://c1/node/%6Eode1` for `node1`.

Fix: one encoder in the library with test vectors: per-byte ASCII lowercase first, then
percent-encode every byte outside the unreserved set with uppercase hex; refuse empty names
and a length cap; verifiers compare bytes against the encoder's output. T1 adds the
vectors.

### SR12. Gate-row traps found by probe (low)

- T10 (`design:278`): `X509_pubkey_digest` hashes the public key's BIT STRING contents,
  not the SubjectPublicKeyInfo DER. Probe: `reactor_probe` prints SPKI `9415a031…` (4.0.3)
  and `378cb413…` (3.6.5) for `node1.pem`, where `openssl x509 -pubkey | openssl pkey
  -pubin -outform DER | openssl dgst -sha256` gives `228fde16…` and `ff63cc27…` for the
  same leaf. The library hashes `i2d_X509_PUBKEY(X509_get_X509_PUBKEY(x))`, or T10's
  equality fails and a consensus SPKI pin computed one way never matches the tool's.
- T1 (`design:269`): "the serial is 16 bytes". With the top bit clear the first byte is
  0x00 in 1 of 128 issuances and DER drops it: probe `0x00112233…` encodes in 15 bytes.
  Set bit 6 of the first byte (probe `0x4011…`: 16 bytes) for a fixed length at 126 bits.
- T5 (`design:273`): "refused at notAfter plus 1 s" holds under both versions; the instant
  of notAfter does not: 3.6.5 refuses it, 4.0.3 accepts it (probe 6c, and `-attime`).
  Assert plus 1 s only.
- Positive controls that hold as written: every role under `-x509_strict -purpose` and the
  CA under `-x509_strict`; the two-anchor bundle in reactor's Peer role and both single-CA
  refusals (probe 1a-1c); the rogue CA (2e, 2f); `pathlen:0` against an intermediate (5a,
  error 25); the validity edges at notBefore less 1 s (6a, error 9) and at notBefore (6b).

### SR13. The threat model omits actors and overstates one row (low)

- Row 1 (`design:132`), "peers verify both ends": Xapiand has no peer leg; node-to-node
  traffic is in clear (`transport-security.md:32-34`). Scope the row to the API and to
  Detent's peers.
- Missing: a client holding a valid client certificate (SR2); a writer of a consumer's
  certificate directory, who replaces that node's `trust.pem` (Detent's pins hold;
  Xapiand's clients and the future URI check do not) and who redirects `--tls-key` through
  the symlink `src/tls.cc:30-32` follows; the attacker during either rotation (SR8); and
  the statement that a planned rotation evicts nobody.

Fix: add the rows, in the actor, capability, decision, residual form the table already
uses.

## Blocking items

None. The findings are conditions on revision 1 because no certificate exists yet; C1 and
C2 become blocking the moment a leaf is issued for anything but a test fixture.

## Conditions

1. C1 (SR1): the CN is the identity URI or absent; `bind_hostname` sets
   `X509_CHECK_FLAG_NEVER_CHECK_SUBJECT`; T1 refuses a hostname equal to the CN.
2. C2 (SR2): the role is in the URI path (`/node/`, `/client/`); the identity check
   requires the prefix and the direction's EKU; T3 covers a `client` leaf.
3. C3 (SR3): the index records SANs; `sign` warns on a SAN shared across active
   identities; the threat table and Q5 state the exposure of a stolen key in days.
4. C4 (SR4): a controlled 0640 in `src/tls.cc` or a documented init-container path; flag
   1 states the `fsGroup` behavior; T2 adds the 0640 case.
5. C5 (SR5): scrypt parameters stated; a minimum passphrase enforced; `--passphrase-file`
   checked like `ca.key` and refused inside `DIR`; unencrypted keys flagged; no core dumps
   and wiped buffers, with a T8 case.
6. C6 (SR6): `sign --name` required; the request's SPKI printed on both hosts and
   checkable; the request parsed before the passphrase prompt with the minimal surface; T4
   adds a substituted request.
7. C7 (SR8): a backup and restore line; D8 on a fresh host with the old bundle removed, not
   appended; the request source for kept identities; an overlap warning; the statement that
   planned rotation evicts nobody.
8. C8 (SR7, SR9 to SR13): directory and temp-file checks with `openat` and `O_EXCL`; an
   RSA warning and the forward-secrecy statement; the cluster recorded at `init`, with
   optional name constraints; the URI encoder with vectors; `i2d_X509_PUBKEY` for the SPKI
   digest, bit 6 in the serial, and T5 asserting notAfter plus 1 s only; the threat-model
   rows.

Not run: no implementation exists, so no file-system refusal, lock, atomic write, or
passphrase path was exercised; no Xapiand build or harness; no Linux or FreeBSD; the
Kubernetes behavior is read from the kubelet's sources, not run in a cluster;
consensus's `authenticated_voter` was not linked (its DER pin is `X509_digest`,
`tls.h:12-19`, which the probe computes directly); Detent was read only in the cited
documents and its test fixture; no listener was started and no handshake crossed a socket.
