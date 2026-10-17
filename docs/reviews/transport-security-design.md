# Xapiand review: transport security design

- Date: 2026-10-17.
- Reviewed: `docs/design/transport-security.md`, committed at 5e112d6b8 as
  `TRANSPORT_SECURITY.md` and moved unchanged at 38b79ed37 (`diff` shows no change).
  Xapiand at 38b79ed37; reactor 56783e5; http cf84df3 (40fc0d7 when the review started;
  cf84df3 adds `Request::remote`); cluster 315d8c8; consensus 01382e5; Detent 8412624,
  docs only, read-only.
- Checked against: the design's D1 to D8, steps, rows T1 to T6, and open questions;
  `AGENTS.md`, `ARCHITECTURE.md`, `docs/dev/testing.md`, `docs/dev/xapian-fork.md`;
  Detent's `docs/AUTHORITY_TRANSPORT.md`, `docs/ASYNC_AUTHORITY_ARCHITECTURE.md`,
  `docs/RELAY.md`, `docs/AUDIT.md`, `src/detentd/CLUSTERING.md`.
- Checked by: reading every path cited below, and two probes with their outputs in
  `.scratch/review-transport-security/probes/`: `inet_aton_probe.c` (how
  `host_address()` parses `--bind-address` forms) and `tls12_default_ciphers.sh` (the
  TLS 1.2 suites `reactor::tls` leaves enabled).
- Citations: `design:N` is line N of `docs/design/transport-security.md`. `reactor/`,
  `http/`, `cluster/`, and `consensus/` are siblings under `~/code/Kronuz/`; `Detent/` is
  `~/code/Detent/`; `asio/` is the fetched standalone Asio.

## Verdict

**REJECT: D2's transport and name binding, D3's replay rule, D5's bootstrap, rotation,
and enrollment, and D7's loopback exemption do not hold as written, and the gate rows do
not prove their targets.**

Kept: TLS on the three TCP services, one sealed channel for gossip, refusal of mixed
clusters, and Detent's off-loopback rule. The design misreads where the bytes flow: two
of D2's four legs never pass through a reactor stream, because the remote client lives in
the vendored Xapian code and replication writes raw bytes to the socket fd. Discovery's
identity and replay semantics are weaker than TLS's, and the design treats them as equal. Step 2 turns Xapiand's default from "start N nodes and they find each
other" into "start N nodes and get N clusters".

What holds, and reproduces:

- The "Why" defaults match the code: an empty `--bind-address` binds every IPv4
  interface (`src/opts.h:51`, `reactor/reactor.h:70,272-274`); HTTP 8880, remote 9880,
  replication 7880, discovery `239.192.168.1:58880` (`config.h.in:282-286`); the API
  checks no credentials (no `Authorization` handling under `src/server/`).
- `reactor::tls` sets what D1 cites: a TLS 1.2 floor, no renegotiation, no early data, a
  64 KiB certificate-list cap (`reactor/reactor_tls.h:61-66`). The Server role needs no
  authority (`reactor/reactor_tls.h:54-60`, `reactor/test/tls_test.cc:130,136-139`).
- http's TLS factory has a handshake deadline and admission caps
  (`http/http_asio_tls.h:22-66,85-96`).
- The remote and replication framings are stream-agnostic
  (`src/server/remote_protocol_service.h:79-155`,
  `src/server/replication_protocol_service.h:92-166`): TLS needs no wire-protocol change.
- The Bus frame (`cluster/bus.h:29-32`) has room for a sealed variant. A 96-bit nonce of
  a random 64-bit incarnation and a 32-bit counter is unique under one key, provided the
  counter is atomic and never wraps (TS-R4).
- Detent's authority and relay rules are cited correctly
  (`Detent/docs/AUTHORITY_TRANSPORT.md:13,47`, `Detent/docs/RELAY.md:65`,
  `Detent/docs/ASYNC_AUTHORITY_ARCHITECTURE.md:69`).

What blocks: TS-R1 to TS-R15, one line each under "Blocking items". TS-R16 and TS-R17 are
conditions. D1 does not depend on the rest; with TS-R11 and the T1 part of TS-R15 fixed it
can return for a re-check as its own slice.

## Rulings

| Decision | Ruling | Findings |
|---|---|---|
| D1 HTTPS for the API | Sound with changes: `--tls-ca` and the step-1 listener rule are wrong or missing | TS-R11, TS-R16 |
| D2 Mutual TLS between nodes | Wrong as written: two legs are not on reactor, and the name check binds an address | TS-R1, TS-R2 |
| D3 Sealed discovery | Mechanism sound, rules wrong: replay, header, nonce, key lifecycle | TS-R3, TS-R4, TS-R5 |
| D4 Mixed clusters refused | Sound with changes: D2 and D3 must be one switch | TS-R13 |
| D5 First-start certificates | Wrong as written: bootstrap, enrollment, rotation, authority location | TS-R5 to TS-R8 |
| D6 TLS off loopback | Sound with changes: classify by bound endpoint; discovery ignores `--bind-address` | TS-R10 |
| D7 API token | Wrong as written: the loopback exemption is a bypass; the libraries cannot express it | TS-R9 |
| D8 Reload | Sound with changes: no mechanism, misses trust bundles, keeps revoked sessions | TS-R12 |

| Row | Ruling | Findings |
|---|---|---|
| T1 | Target right; the procedure cannot run as written | TS-R15 |
| T2 | Incomplete; refusals have no positive controls | TS-R15, TS-R3 |
| T3 | Claims failover; the script tests none | TS-R15 |
| T4 | "Never moves the authority key" is not falsifiable as specified | TS-R15, TS-R7 |
| T5 | Names the wrong layer; plain-only, client-bound harness; no cluster workload | TS-R15 |
| T6 | CI's sanitizer jobs run none of T2 to T4 | TS-R15 |

Steps: step 1 as listed needs changes in reactor (TS-R11), http (TS-R12), cluster
(TS-R4), and the Xapian fork or its replacement (TS-R1). List them as step-1
prerequisites.

Open questions (design:127-134):

1. Per-user tokens: not yet. Step 2 needs one cluster-wide token set with ids, so a
   token rotates by add-then-remove; per-user tokens arrive with per-index permissions.
2. Leaf pins: no. Pins fit Detent's fixed voters (`Detent/docs/AUTHORITY_TRANSPORT.md:47`,
   `consensus/include/consensus/transport/tls.h:21-35`) and churn with D5's renewals. Bind
   authority, cluster, and node name in the SAN, and enforce uniqueness at issuance
   (TS-R2).
3. Authority down: do not copy the key to a second serving node automatically. Use
   escrow and restore, a renewal and expiry policy, and a recovery procedure; optionally
   an offline root with online issuing intermediates (TS-R8).

## Findings

### TS-R1. D2's transport claim fails for the remote client and the replication writer (blocking)

Evidence:

- The remote-protocol client is Xapian's: `src/database/shard.cc:341,454` call
  `Xapian::Remote::open` and `open_writable`, which end in `RemoteConnection` doing raw
  `read`, `write`, and `poll` on file descriptors (`src/xapian/net/remoteconnection.h:53,61`,
  `src/xapian/net/remoteconnection.cc:204,238,295`). `docs/dev/testing.md:140` names
  `src/xapian/backends/remote/` as the client. None of it touches reactor or Asio.
- Replication handlers write framed replies and whole database files straight to the
  socket fd from the offload pool (`src/server/replication_protocol_service.h:30-31,207,248`;
  `src/server/replication_protocol_views.cc:174-190,1028,1041-1047`). Under TLS those
  writes inject plaintext into the record stream.
- Both server coroutines take `asio::ip::tcp::socket` concretely
  (`src/server/remote_protocol_service.h:72,84,160`;
  `src/server/replication_protocol_service.h:96,170,201`).

Why it matters: D2 is the data-plane protection, and as written it covers only the
remote server. TLS on the remote client is a patch to the vendored fork
(`docs/dev/xapian-fork.md` rules) that strata's plan later deletes (`ARCHITECTURE.md:13-17`),
or a different client. Neither is in the design or the step-1 scope.

Required change: name the mechanism for each of the four legs: remote server, remote
client, replication server, replication client. Template the server coroutines on the
stream. Route replication writes through the TLS stream, with no raw-fd writes and with
`SSL_ERROR_WANT_READ` handled inside writes. Choose a `xapian(net)` fork patch that also
carries the expected peer name, or a replacement client, and state how the choice meets
the strata engine seam. Give the cluster listeners the handshake deadline and admission
caps that only http has today (`http/http_asio_tls.h:22-66`).

### TS-R2. D2's name check binds an address, so one node's certificate can stand in for another's (blocking)

Evidence:

- The check compares the certificate with "the node name discovery announced for that
  address" (design:51-52). Several nodes share an address: same-host nodes take
  incremented ports (`src/manager.cc:936-968`), and `harness/cluster_check.sh:86-96` runs
  every node on one host. An acceptor sees a source address with an ephemeral port, which
  on multi-homed or NATed hosts also differs from the announced one (`host_address()`
  announces the first non-loopback IPv4 interface, `src/manager.cc:386-393`).
- The name-to-address map comes only from gossip payloads; the UDP source is ignored
  (`src/server/discovery.cc:240`). Any key holder (or anyone, with D2 but no D3, TS-R13)
  can announce "B at A's address". Node A's certificate then matches A's announced name
  at that address, and a dialer that wanted B accepts A.
- reactor has no peer-name helper; its comment points at pinning (`reactor/reactor_tls.h:79-81`),
  which lives in consensus as pin to node id (`consensus/include/consensus/transport/tls.h:47-60`).
- No certificate profile: which field carries the name, the case rule (node names
  compare lowercased, `src/node.h:141`), wildcards, the cluster; no rule that the
  authority refuses a name it already issued.

Why it matters: the decision's stated purpose fails, and the acceptor-side check either
refuses legitimate peers or checks nothing.

Required change: the dialer verifies that the certificate identity is the node it meant
to reach (the name from the cluster database or `Endpoint`), whatever the address. The
acceptor takes the identity from the verified certificate, never from the address. One
profile: a SAN URI such as `xapiand://<cluster>/node/<lowercased name>`, exact byte
match, no wildcards, no CN fallback, binding the cluster as Detent binds cluster and epoch
(`Detent/docs/ASYNC_AUTHORITY_ARCHITECTURE.md:69`). The authority refuses an issued name
unless a token explicitly replaces it. Put the check in `reactor::tls` next to
`bind_hostname`, with tests, and document the profile for bring-your-own certificates.

### TS-R3. D3's replay window does not survive a receiver restart (blocking)

Evidence:

- Incarnations are random (design:59-60), so a receiver cannot order them, and
  per-incarnation state lives in memory. A restarted receiver, or one that evicted an
  entry, accepts any recorded datagram once.
- Every graceful stop multicasts CLUSTER_BYE with the node's name, address, and ports
  (`src/server/discovery.cc:193-195`). A receiver drops the named node by name alone
  (`src/server/discovery.cc:449-464`, `src/node.cc:419-440`) and re-runs elections
  (`src/server/discovery.cc:466-482`). A rolling restart restarts every receiver, so every
  recorded BYE becomes replayable to every node.
- Raft term and vote are volatile (`cluster/raft.h:723`), so the term check gives a
  restarted node no floor either.
- T2's "a replayed datagram" (design:121) passes against a live receiver and never
  reaches this case.

Why it matters: "Without the key, a node cannot ... replay an announcement"
(design:62-63) is false. Anyone who recorded sealed traffic forces node drops and
elections after any restart, without the key.

Required change: add freshness. Seal a sender timestamp and reject datagrams outside a
stated window, with the clock-skew assumption and its failure log; or persist a
monotonic incarnation per node, let a newer one supersede, and keep the timestamp for
first contact. Keep per-incarnation windows at least as long as the freshness window, so
eviction after it is safe. Add T2 cases: replay to a restarted receiver, and replay of a
previous incarnation's BYE.

### TS-R4. D3's frame rules leave the header, the nonce, downgrade, and size open (blocking)

Evidence:

- The clear header is `[major][minor][type][token]` (`cluster/bus.h:32`). The design
  authenticates only the cluster name and type (design:60), so the version bytes, and any
  future key id, sit outside the associated data.
- Today a frame passes with any version up to ours (`cluster/bus.h:104`); the design does
  not say a keyed Bus refuses unsealed frames of an older version.
- `Bus::send` runs on two threads: the bus reactor (Raft, gossip) and the manager thread,
  through `Discovery::stop()` sending CLUSTER_BYE before `raft_.stop()`
  (`src/manager.cc:1334`, `src/server/discovery.cc:194-197`). reactor documents send as
  safe only alongside the receive loop and asks single-threaded protocols to post sends
  onto `io()` (`reactor/reactor_udp.h:153-155`). A plain counter bumped from both threads
  repeats a nonce under ChaCha20-Poly1305. Counter exhaustion at 2^32 is unspecified.
- "not above the last seen ... within a 1,024-entry window" (design:61-62) states a
  strict rule and a sliding window at once, and "per sender" has no key. Same-host nodes
  all bind `0.0.0.0:58880` with `SO_REUSEPORT` and send from the same address and port
  (`src/server/discovery.cc:133-140`, `reactor/reactor_udp.h:96-98`,
  `harness/cluster_check.sh:18-21`), so the UDP source cannot key the window.
- The receive buffer is one 1,500-byte datagram (`reactor/reactor_udp.h:69`; discovery
  sets no `max_datagram`). Sealing adds at least 28 bytes.
- Every default-configured cluster shares group, port, and the name `Xapiand`
  (`config.h.in:281-283`). A keyed node receives their datagrams, and D4's "logs why"
  (design:67-68) floods without filtering and rate limits.

Why it matters: each item is a way to get the cryptography wrong or bypass it, and none
has a row.

Required change: the associated data is the whole clear header (version, type, token,
key id) plus the nonce fields. A keyed Bus accepts only the sealed format. One atomic
counter per Bus, or all sends posted to the bus thread. A fresh CSPRNG incarnation before
the counter wraps. Replay state keyed by incarnation as a sliding bitmap: accept a counter
above the highest, or within 1,024 below it and unseen; update only after the tag
verifies. The sender refuses a sealed frame above `max_datagram`. Filter on the clear
token before opening and rate-limit the refusal log. Ship the OpenSSL dependency as an
optional cluster target, like `reactor::tls`.

### TS-R5. Rotating the authority does not evict a compromised node (blocking)

Evidence:

- Renewal runs over mutual TLS (design:84-85): the compromised node presents its
  still-valid certificate and renews into the new authority like every other node.
- The overlap keeps old certificates valid "for one renewal period" (design:86-87), 60
  days at a 90-day lifetime.
- Established connections keep their context (design:107), and TLS checks validity only
  at the handshake.
- The discovery key is handed out once at join (design:82) and has no key id or
  rotation. The compromised node keeps reading and forging gossip and Raft: a group key
  authenticates membership, not the sender, whose identity is the serialized node inside
  the payload (`src/server/discovery.cc:914-915`, `src/node.cc:65-76`).
- When the compromised node is the authority node (design:82-83), rotation cannot start
  there.

Why it matters: the threat model leaves compromised nodes out of scope on the strength of
this procedure (design:33), and the procedure does not remove them.

Required change: an emergency procedure. Issue a new authority and a new discovery key,
with a key id in the frame, only to an explicit list of surviving nodes, by fresh
enrollment or allowlisted renewal; no overlap in that case. Close sessions whose peer
chain no longer verifies. Cover the authority-node-compromised case. Add a row.

### TS-R6. Starting a node without flags splits the cluster, and so does upgrading one (blocking)

Evidence:

- "the default start of a node needs no flags: it generates what it needs"
  (design:97-98), and the first node creates an authority (design:73-75). A node without
  `<data>/.tls/` and without a token cannot tell "first node" from "new node", so each
  creates its own authority and discovery key.
- The documented StatefulSet starts three replicas with identical arguments and no token
  (`docs/src/content/docs/reference-guide/kubernetes.md:52,65-68`); the container
  entrypoint adds only user, group, name, cluster, and data directory
  (`contrib/docker/xapiand/entrypoint.sh:89-95`).
- An existing plain cluster upgraded to step 2 has data and no `.tls/` on every node.
- The result is N one-node clusters with one name on one group and port, each dropping
  the others' datagrams: the "looks healthy" failure D4 exists to prevent (design:67-68),
  in a sealed form.

Why it matters: it breaks Xapiand's main deployment path without an error.

Required change: creating an authority takes an explicit flag (for example
`--bootstrap`). Without it and without a token, a node refuses to start and says how to
enroll. The clear cluster token lets a starting node detect sealed traffic for its name
and refuse to bootstrap. A node with a cluster database and no `.tls/` refuses step 2
until migrated; write that migration (one node bootstraps, the rest enroll, a flag-day
restart). Give StatefulSets an unattended path: bring-your-own under the TS-R2 profile, or
a mounted bootstrap secret with its risk stated. Add rows.

### TS-R7. D5's enrollment protocol leaves open where MITM resistance and single use are decided (blocking)

Evidence:

- The token holds a secret and a fingerprint (design:76-78), but no issuer address or
  name. The joiner has no discovery key yet, so gossip cannot locate the issuer.
- The joiner has no certificate, and the Peer role refuses clients without one
  (`reactor/reactor_tls.h:74`), so enrollment cannot use the remote or replication
  listeners. The design names no listener.
- "checks the presented authority against the token's fingerprint" (design:80-81): the
  authority certificate is public, since any node may send it in its chain and TLS 1.2
  sends chains in clear. A check that the chain contains a certificate with that
  fingerprint is forgeable. The fingerprint must pin the trust anchor the leaf verifies
  against, and the design must say whether it hashes the DER or the SPKI.
- The token binds no node name and the authority has no uniqueness rule (TS-R2), so a
  token holder picks any name, an existing node's included.
- Single use (design:77-78) has no rule for persistence across restarts, atomic consume
  under concurrent redemptions, or a lost signed response, which burns the token.
- No CSR policy: the authority must set its own extensions (CA:FALSE, EKU, SAN), ignore
  the request's, and cap the request's size.
- `xapiand --enroll` runs as a second process "on an existing node" (design:76); how it
  reaches the running node's token store is unstated.
- `--enrollment-token` on the command line is visible to every local user through the
  process table for 30 minutes. Detent keeps credentials in files with checked ownership
  and mode (`Detent/docs/RELAY.md:15`).
- Tokens are "printed" (design:76, design:101). With `--detach` the logger writes to a
  file (`src/opts.cc:522-528`); Detent's rule is to never log secrets
  (`Detent/docs/ASYNC_AUTHORITY_ARCHITECTURE.md:69`).
- A notBefore of "now" on the authority fails on a joiner whose clock lags.

Why it matters: these are the decisions that make enrollment resist a man in the middle
and stay single-use; leaving them to the implementation leaves the security to it.

Required change: specify the token (secret, authority SPKI hash, issuer address and name,
the new node's name, expiry); the listener and its role; the verification algorithm;
an atomic, persisted consume with idempotent retry for the same request key; the CSR
policy; token input by file or environment; printing to stdout only; a backdated
notBefore. Each gets a T4 case (TS-R15).

### TS-R8. The authority lives in the data directory of one serving node, with no recovery (blocking)

Evidence:

- The authority key and node keys go to `<data>/.tls/` (design:73-75), and `<data>` is
  the database root (`src/opts.cc:379`). Every backup, snapshot, or volume copy of a node
  carries the authority key, and copying a data directory to seed a node clones its
  identity.
- Only the authority node enrolls and renews (design:82-84). Nodes enrolled together
  expire together. An authority outage past 30 days (90-day life, renewal at day 60)
  expires them all; losing that disk ends the cluster's TLS within 90 days. Open question
  3 (design:133-134) is this problem.
- The authority key sits in the memory of the process that parses every HTTP request, so
  any disclosure or code-execution bug in that surface exposes it; the surface already
  carries known parser defects (`ARCHITECTURE.md:592-595`).
- No expiry metric, no alert, and no path for a node whose own certificate has expired,
  which can no longer renew over mutual TLS.

Why it matters: the cluster's identity has one copy, in the most-copied directory on the
most-exposed process, and its loss is a dated outage.

Required change: answer open question 3 as ruled above. An explicit authority directory
outside `<data>`, or a stated backup exclusion; an operator-run escrow export and
restore; renewal retries with jitter; an expiry metric and a warning at five sixths of
life; re-enrollment for an expired node; the recovery procedure when the authority is lost
(new authority, re-enroll every node). Refuse keys that are group- or world-readable or
reached through symlinks, as Detent does (`Detent/docs/RELAY.md:15`), and write `.tls/`
files atomically. Add a row with the authority down across a renewal.

### TS-R9. D7's loopback exemption is a bypass, and the libraries cannot express D7 yet (blocking)

Evidence:

- Exempting loopback clients (design:102-103) trusts every request when Xapiand sits
  behind a same-host reverse proxy or a service-mesh sidecar, and every request from any
  server-side request forgery on the host. Xapiand checks no `Host` header (no match
  under `src/server/`), so a browser on the host reaches `localhost:8880` through DNS
  rebinding.
- Every gate procedure talks to localhost (`harness/e2e_capture.sh:59-73`,
  `harness/cluster_check.sh` curls `localhost`), so no row would exercise the token.
- "A node generates an API token" (design:101) contradicts "one per cluster"
  (design:129). Per node means N tokens and no distribution path.
- No token specification: entropy; hash (SHA-256 suffices for a random token of 128 bits
  or more); constant-time comparison (consensus uses `CRYPTO_memcmp`,
  `consensus/include/consensus/transport/tls.h:34,57`); rejection before the body is read
  (`http::RequestRejected`, `http/docs/TRANSPORT.md:28`); rotation; where it prints
  (TS-R7).
- "clients with a certificate signed by the cluster authority are accepted without it"
  (design:103-104): the Server role never requests a client certificate (`verify_none`)
  and the Peer role demands one from every client (`reactor/reactor_tls.h:74-76`). http
  hands handlers no TLS peer state. `Request::remote` arrived during this review (http
  cf84df3, `http/http_message.h:99`) as a string, so a v4-mapped `::ffff:127.0.0.1` must
  be parsed, not compared. Node and client certificates would share one authority with no
  role marker, so a client certificate could pass as a node under D2.

Why it matters: D7 exists because D6 protects traffic, not access (design:100-101), and
the exemption hands access back to anything that can reach loopback.

Required change: drop the exemption, or make it an explicit opt-in with a `Host`
allowlist. One cluster-wide token set with ids, distributed at enrollment. State the
entropy, SHA-256 digests, `CRYPTO_memcmp` across every stored digest (Detent's full scan,
`Detent/docs/AUTHORITY_TRANSPORT.md:13`), and rejection at header admission. Add a reactor
mode that requests and verifies an optional client certificate, and an http hook that
exposes the verified identity. Separate roles in the SAN (`.../node/...` against
`.../client/...`). Add rows.

### TS-R10. D6 classifies listeners by the wrong address (blocking)

Evidence:

- Discovery ignores `--bind-address`: its socket binds `0.0.0.0:58880`
  (`src/server/discovery.cc:133-140`, `reactor/reactor_udp.h:58,96-98`) with multicast TTL
  3 (`src/server/discovery.cc:138`). `--bind-address 127.0.0.1` leaves gossip open to the
  segment, to two router hops beyond it where multicast is routed, and to unicast from any
  routable host.
- `host_address()` treats `inet_aton(...) == -1` as failure (`src/manager.cc:372`), but
  `inet_aton` returns 0. The probe (`inet_aton_probe.out`) returns 0 for `::1`,
  `localhost`, and `::ffff:127.0.0.1`, so those values announce the first non-loopback
  IPv4 interface (`src/manager.cc:389`) while the TCP listeners bind `::1` through
  `asio::ip::make_address` (`reactor/reactor.h:272-274`), which rejects names outright.
- In Asio, IPv4 loopback is 127/8, IPv6 loopback is `::1` only, and a v4-mapped address
  is not loopback (`asio/ip/impl/address_v4.ipp:86-89`,
  `asio/ip/impl/address_v6.ipp:106-147`).

Why it matters: the rule that decides when plaintext is allowed reads a string, an
announced address, or nothing, depending on the listener.

Required change: decide per listener from the bound endpoint, never from the string or
the announced address. Discovery counts as loopback only when `--discovery-interface` is
loopback; state the TTL that goes with it. Refuse to start when the announced and bound
addresses disagree. State the v4-mapped rule: normalize, or treat as non-loopback. Add a
row.

### TS-R11. D1 claims library support that does not exist (blocking)

Evidence:

- "`--tls-ca FILE` also verifies client certificates, for mutual TLS from clients that
  have them" (design:43-44). The Server role loads the CA and sets `verify_none`; the Peer
  role refuses any client without a certificate (`reactor/reactor_tls.h:72-76`). Neither
  offers optional client verification.
- "bounded handshakes: reactor's settings" (design:45-46): the deadline and caps are
  http's (`http/http_asio_tls.h:85-96`: 5 s, 64 handshakes, 1,024 connections); reactor
  sets only the protocol floor and options (`reactor/reactor_tls.h:61-66`).
- Xapiand builds the HTTP service with the four-argument constructor and no connection
  cap (`src/manager.cc:985`; a zero cap disables admission, `reactor/reactor.h:365-367`),
  and never sets a request timeout.
- "The plain listener stays available only as D6 allows" (design:46): D6 arrives in step
  2, so step 1's listener behavior is undefined.
- Under D5, which certificate the API presents, which names it carries, and how clients
  get the private CA are unstated. A client that verifies hostnames fails on SANs the node
  cannot know.

Why it matters: D1 is the slice closest to approval, and two of its stated properties
need code that the design says is already there.

Required change: drop `--tls-ca` from D1 or add a reactor mode for optional and required
client verification, with tests. Cite http as the owner of handshake bounds and pass a
connection bound through the six-argument constructor (`http/http_asio.h:826-833`). State
step-1 listener behavior; TLS replacing plain on `--port` is the simple rule. For step 2,
add a `--tls-name` for SANs and a way for clients to fetch or pin the CA.

### TS-R12. D8 has no mechanism, misses the trust bundle, and keeps revoked sessions (blocking)

Evidence:

- The TLS factory captures one `shared_ptr<asio::ssl::context>` for its lifetime
  (`http/http_asio_tls.h:92`), and the service takes the factory only at construction
  (`http/http_asio.h:826-833,877`). Nothing swaps it.
- D8 reloads only `--tls-cert` and `--tls-key` (design:106-107). D5's renewals need the
  cluster contexts reloaded, and rotation needs every node's CA bundle reloaded.
- "existing connections keep theirs" undoes revocation (TS-R5).
- SIGHUP is ignored today (`src/main.cc:289`); a reload must go through the self-pipe
  like every other signal (`ARCHITECTURE.md:138-142`).
- A mismatched certificate and key throw in `configure` (`reactor/reactor_tls.h:70`);
  the design does not say the old context stays.
- No row.

Why it matters: renewal and rotation both depend on D8.

Required change: a context provider in http and in the cluster listeners, swapped
atomically per accept. Reload chain, key, and CA bundle for every listener. On any
failure, keep the old context and log. When the trust bundle changes, close sessions
whose peer chain no longer verifies. Keep SIGHUP ignored when no TLS is configured. Add a
row.

### TS-R13. D4 admits half-secured clusters (blocking)

Evidence:

- "a node configured with D2 or D3" (design:67) allows D2 without D3, and D3 without D2.
- D2 without D3 leaves membership and Raft forgeable by anyone who can send a datagram
  to port 58880. Names, addresses, and ports come from the payload
  (`src/server/discovery.cc:240`, `src/node.cc:80-101`); BYE drops a node by name
  (`src/server/discovery.cc:449-464`); and once a primary has gone unseen for
  `--database-stall-time` (300 s, `src/opts.cc:138`; `src/index_resolver_lru.cc:216-225`),
  the leader elects a new one from voter-reported revisions (`src/server/discovery.cc:747-796`),
  which a forger supplies.
- D3 without D2 leaves documents and replication in clear.
- Plain to secure is then a flag-day restart of the whole cluster, which the design does
  not mention.

Why it matters: "all-or-nothing per cluster" (design:66) is the design's own rule, and
the either-or wording breaks it.

Required change: one cluster-security switch that means D2 and D3 together, with partial
configurations refused at startup. Document the flag-day migration and add a row that
converts a plain cluster's data directories.

### TS-R14. The threat model overstates some protections and omits actors (blocking)

Evidence:

- Discovery is exposed beyond "the segment" (design:15): bound to `0.0.0.0`, TTL 3, open
  to unicast (TS-R10).
- "cannot ... read membership" (design:62): under D3, every member's source IP, the
  cluster name, each message type, and their timing stay in clear (`cluster/bus.h:32`);
  `harness/discovery_sniff.py` decodes that header today.
- Denial of service is scoped out "beyond what the existing connection caps and
  handshake deadlines bound" (design:36-37). No TCP service has a connection cap
  (`src/manager.cc:985,1004,1019`; `reactor/reactor.h:365-367`); neither framing bounds a
  message's length (`src/server/remote_protocol_service.h:136-149`,
  `src/server/replication_protocol_service.h:143-164`, `ARCHITECTURE.md:537-541`); and
  handshake deadlines exist only in http.
- Missing actors: other processes on the host and co-tenant containers, which D6 and D7
  trust through loopback; browsers on the host (TS-R9); secrets at rest in `<data>/.tls/`
  (TS-R8).
- "Detent ... requires TLS on every non-loopback listener" (design:17-18): Detent's
  legacy UDP and HTTP profile, the one that rides `cluster::Bus`, has no TLS
  (`Detent/docs/AUDIT.md:9`). The rule holds for its authority profile and relay
  (`Detent/docs/AUTHORITY_TRANSPORT.md:47`, `Detent/docs/RELAY.md:65`).

Why it matters: the decisions are only as sound as the attacks they were weighed against.

Required change: rewrite the threat model as a table of actor, capability, the decision
that stops it, and what remains. Name the residuals, such as traffic analysis and insider
impersonation in gossip.

### TS-R15. The gate rows do not prove their targets, and most decisions have none (blocking)

Evidence and required changes:

- T1: `run_all.sh e2e` boots its own node with fixed flags (`harness/e2e_capture.sh:59`),
  probes readiness over plain HTTP (`harness/e2e_capture.sh:68`), and the collection
  carries no scheme (`docs_to_postman.py:172-173,268`). The harness needs node flags, an
  `https` scheme, and a CA for newman.
- T2: a refusal case passes when the port is merely closed. Pair each refusal with an
  accept case that differs in one variable, and assert the logged reason. Add: replay to
  a restarted receiver; altered header bytes; an unsealed datagram to a keyed node;
  expired and not-yet-valid certificates; TLS 1.1 and renegotiation attempts; a
  certificate for another cluster from the same authority; a client certificate presented
  as a node; exhausted handshake slots.
- T3: `harness/cluster_check.sh:138-193` checks membership, one leader, a cross-node read,
  and distributed search; it has no failover. Add one (kill the leader and a primary;
  reads and writes resume), and run nodes on distinct addresses as well as on one shared
  address.
- T4: "never moves the authority key" needs a falsifiable form: parse the enrollment
  response and check it holds only the certificate, its chain, and the discovery key. Add
  concurrent redemption (exactly one wins), a restart inside the validity window, expiry at
  29:59 and 30:01 under an injectable clock, a duplicate name, and a CSR asking for
  CA:TRUE.
- T5: "the load layer" is `index_fortune`, a functional bulk load
  (`docs/dev/testing.md:31`). The benchmark, `loadtest.py`, speaks plain HTTP only
  (`harness/loadtest.py:40-41`) from a threaded CPython client that can saturate before
  the server, and the row has no cluster workload although `cluster_bench.sh` exists
  (`docs/dev/testing.md:289-296`). Measure server CPU seconds per 1,000 requests and per
  GiB replicated, plain against TLS, interleaved, with trials and spread stated so 5% sits
  above noise. Add RSS at the TLS connection cap, and convert to cores at the benchmark's
  rate.
- T6: CI's sanitizer jobs run smoke, load, and e2e on Linux only
  (`.github/workflows/ci.yml:99-102,123-131`); the cluster layer is not in CI, so T2 to T4
  never run there. Say where the multi-node cases run under ASan, UBSan, and TSan.
- No rows: D3 confidentiality (`harness/discovery_sniff.py` sees only ciphertext after
  the header), D6 classification, D7, D8, rotation, renewal at two thirds of life, the
  authority down, the step-2 upgrade, handshake lockout.

Why it matters: a row that cannot run, or cannot fail, approves nothing.

### TS-R16. Limits the design calls bounds (condition)

- http admits 64 handshakes with a 5 s deadline (`http/http_asio_tls.h:85-87`). About 13
  stalled connections per second from one host hold every slot; nothing limits a single
  source.
- With no request or idle timeout, 1,024 idle authenticated connections hold the TLS
  connection cap. `opts.max_clients` is computed (`src/main.cc:346-372`) and applied to no
  service.
- TLS 1.2 keeps OpenSSL's DEFAULT list, which includes four static-RSA suites without
  forward secrecy (`tls12_default_ciphers.out`, OpenSSL 3.6.5 and 4.0.3). With a
  bring-your-own RSA certificate, a recorded session that negotiated one of them decrypts
  after a later key theft. Restrict TLS 1.2 to ECDHE with AEAD in
  `reactor::tls::configure`.
- Session tickets use one key per context for the process's lifetime; rotate them with
  D8, or disable tickets for TLS 1.2.
- Per-connection TLS memory at the 1,024 cap is unstated (T5).
- Replication compresses before TLS (flume, Zstd). Once per-index permissions exist,
  size side channels between tenants need a ruling; record it as a residual.

Condition: state each bound as a residual with numbers; add idle and request timeouts
and per-listener caps to all three TCP services; restrict the TLS 1.2 cipher list.

### TS-R17. Layering and coordination (condition)

- `reactor::pki` (design:70-71,90-92) puts certificate issuance and token redemption in
  the event-loop library. Issuance is application policy; a small separate library keeps
  reactor a runtime.
- The peer-name check belongs in `reactor::tls` beside `bind_hostname`, following
  consensus's verified-peer pattern (`consensus/include/consensus/transport/tls.h:47-60`),
  not as a parallel helper in Xapiand.
- D3 changes the `cluster::Bus` wire format that Detent's legacy profile shares. Detent's
  design keeps legacy wire formats intact (`Detent/docs/ASYNC_AUTHORITY_ARCHITECTURE.md:65`),
  and in that profile Raft carries the writes and keeps volatile state
  (`Detent/src/detentd/CLUSTERING.md:5-9,38-46`), so TS-R3 weighs more there. Keep the
  sealed mode additive and off by default, and coordinate with Detent before landing it.
- Operations: wire sniffing with `harness/proxy` (`docs/dev/testing.md:159`) stops on TLS
  links; state the debugging path. Add refusal counters and certificate-expiry metrics,
  and `[Unreleased]` changelog entries per `AGENTS.md`.

## Blocking items

1. TS-R1: name the TLS mechanism for all four D2 legs, including the Xapian remote client
   and the replication raw-fd writer.
2. TS-R2: bind peer identity to the intended node through one SAN profile, with
   uniqueness enforced at issuance.
3. TS-R3: make replay protection survive receiver restarts with freshness.
4. TS-R4: specify the sealed frame: full-header associated data, atomic counter, sealed
   format only, sliding window, size limit, filtered and rate-limited logs.
5. TS-R5: give a rotation procedure that evicts a compromised node, discovery key
   included.
6. TS-R6: require an explicit bootstrap, refuse tokenless joins, and write the upgrade
   and StatefulSet paths.
7. TS-R7: specify the enrollment token, listener, verification, single use, CSR policy,
   and secret handling.
8. TS-R8: move the authority out of the data directory, add escrow, expiry policy, and
   recovery.
9. TS-R9: remove the loopback exemption, specify the token set, add the reactor and http
   hooks D7 needs.
10. TS-R10: classify listeners by bound endpoint and cover discovery's interface.
11. TS-R11: correct D1's library claims and define step-1 listener behavior and step-2
    API certificate names.
12. TS-R12: give D8 a context-swap mechanism covering trust bundles and session closure.
13. TS-R13: make D2 and D3 one switch and document the flag-day migration.
14. TS-R14: rewrite the threat model as actor, capability, decision, residual.
15. TS-R15: make T1 to T6 runnable and falsifiable, and add rows for every decision.

## Conditions

1. TS-R16: state handshake, idle, and connection bounds as residuals with numbers; add
   timeouts and caps on all TCP listeners; restrict TLS 1.2 to ECDHE with AEAD; rotate or
   disable session tickets.
2. TS-R17: keep issuance out of reactor, put the name check in `reactor::tls`, coordinate
   the Bus format with Detent, and cover the operational gaps.

Not run: no build of Xapiand or any library, no unit or harness tests, no TLS handshake,
no multi-node cluster, no benchmark, no Linux or FreeBSD check. D3's replay and nonce
behavior is judged from the code, not exercised. The Xapian remote client was read only at
its I/O layer; `remote_protocol_views.cc` and the Raft delegate were not read in full.
Detent was read only for the cited documents. Whether the docs E2E is green today was not
checked.

## Re-check

- Date: 2026-10-17.
- Reviewed: revision 1 at d847e5f36 (`docs/design/transport-security.md`, 117 lines),
  against this review as committed at 2aaec15a3. Xapiand source at d847e5f36, with its
  pins (`CMakeLists.txt:450,783,805`); reactor 56783e5; http d3b3667 (only pins changed
  since cf84df3).
- Checked by: reading the cited code, plus new probes beside the earlier ones in
  `.scratch/review-transport-security/probes/`: `max_clients_budget.py` (the
  `max_clients` arithmetic of `adjustOpenFilesLimit`), `newman_tls_options.out` (this
  Mac's newman), `openssl_no_ticket_doc.out` (OpenSSL 3.6.5's `SSL_OP_NO_TICKET` text).

### Verdict

**REJECT for step 1, on two small items: D1.3's request deadline cuts long requests and
changes plain HTTP (TS-R18), and the plan omits the dependency step D1 needs (TS-R19).
Everything else is closed, deferred and recorded, or a condition.**

Narrowing step 1 to D1 is right, and the deferral is acceptable: step 1 now claims
nothing between nodes (design:34), and every deferred decision carries its findings
(design:88-97). D1.1, D1.2, D1.4, D1.6, and the new rows hold against the code with the
conditions below. D1.3 does not: it misreads http's request deadline, and it applies new
limits to plain HTTP that D1.1 says stay unchanged.

### Status

| ID | Status | Note |
|---|---|---|
| TS-R1 | Deferred and recorded (design:90); acceptable | Waits on strata replacing the raw-fd paths (design:84-86) |
| TS-R2 | Deferred and recorded (design:90); acceptable | |
| TS-R3 | Deferred; recorded narrower than the finding | design:91 carries "persisted incarnations" only; a restarted receiver also needs a freshness rule for first contact (condition 1) |
| TS-R4 | Deferred and recorded (design:91); acceptable | |
| TS-R5 to TS-R8 | Deferred and recorded (design:93); acceptable | |
| TS-R9 | Deferred and recorded (design:95); acceptable | Step 1 scopes access control out (design:31-33) |
| TS-R10 | Deferred and recorded (design:94); acceptable | Step 1 has no loopback rule |
| TS-R11 | Closed for step 1 | No `--tls-ca` (design:44-46); http owns the bounds and has the six-argument constructor (`http/http_asio.h:826-833`, `http/http_asio_tls.h:85-96`); one listener replaces plain (design:42-44). Xapiand's pins lack both (TS-R19) |
| TS-R12 | Deferred and recorded (design:96); acceptable | The restart rule (design:79-80) matches `http/http_asio_tls.h:92` |
| TS-R13 | Deferred and recorded (design:90,92); acceptable | |
| TS-R14 | Closed for step 1 (design:23-38) | Carried only as "what stays in clear" under D3 (design:91); the deferred design owes the full rewrite (condition 1). The cluster caveat is TS-R24 |
| TS-R15 | Partially closed | T1 to T6 have procedures and accept/refuse pairs (design:101-108); some cases cannot fail as written (TS-R21) |
| TS-R16 | Partially closed | TLS 1.2 restriction and the handshake residuals hold (design:59-60,67-73); remote and replication bounds deferred and recorded (design:97); the request deadline is wrong (TS-R18) and the ticket rule misstates TLS 1.3 (TS-R22) |
| TS-R17 | Open | D1.4 changes `reactor::tls` for every consumer and calls it additive (design:72-73); coordination is unstated, and the deferred layering items are not in the table (TS-R22, condition 1) |

### New findings

#### TS-R18. D1.3's request deadline is an exchange deadline, and D1.3 applies it to plain HTTP (blocking)

Evidence:

- http sets the deadline at the top of each keep-alive iteration
  (`http/http_asio.h:613-615`). It then bounds the wait for the next request's first byte
  (`:619`), streamed body reads (`:653`), dispatch (`:725`), and the reply write
  (`:707,769`). http's own doc says "buffered replies use the remaining budget"
  (`http/docs/TRANSPORT.md:26`). The design says it bounds "only a request's own headers
  and body" (design:63).
- With a 60 s deadline, an idle keep-alive connection closes at 60 s, so the 75 s idle
  timeout never fires and T3's "idle connections closed at 75 s" (design:105) fails
  against the code.
- A whole-database RESTORE streams bodies of any size
  (`src/server/search_views.cc:3075-3087,694-695`), and the 60 s deadline cuts it.
- A dump is built in memory and written as one reply
  (`src/server/search_views.cc:1614-1638`). It, or any handler that runs past the
  remaining budget, can lose its reply after its side effects happened, which invites a
  client to retry a write that is not idempotent.
- D1.3 applies "with or without TLS" (design:55), against "Without them nothing changes"
  (design:44). Plain deployments gain the deadline and a connection cap.
- The connection cap is `opts.max_clients` as `adjustOpenFilesLimit` leaves it
  (`src/main.cc:441-456,486`). It goes negative when the open-files limit cannot be
  raised (probe `max_clients_budget.out`: -3 at 256 files, -4 at 1,024, 291 at 2,048).
  A zero cap means none (`reactor/reactor.h:365-367`), and a negative one wraps when
  cast to `size_t`. `FDS_PER_CLIENT` 2 is a libev-era estimate ("KQUEUE + IPv4",
  `src/main.cc:86`).

Why it matters: as written, step 1 breaks every restore or dump that runs past 60 s, on
every deployment, TLS or not, and T3 cannot pass.

Required change:

1. Apply the new bounds only when `--tls-cert` is given, or declare the plain-HTTP change
   breaking in the changelog. Recommended: TLS only in step 1.
2. Replace the absolute 60 s deadline with an idle timeout between requests and a header
   deadline from a request's first byte to the end of its headers. Put no absolute
   deadline on bodies and replies, or use a minimum-progress rule. Name these as http
   additions (the deadline's start moves to the first byte) and test them in http.
3. Define the connection cap as an explicit number with a floor, and warn or refuse to
   start when the file budget cannot cover it.
4. T3 gets one case per bound. Include a RESTORE body that streams longer than any
   per-request bound, and pin `--max-clients` in the test.

#### TS-R19. Xapiand does not pin the libraries D1 uses, and the plan has no dependency step (blocking)

Evidence:

- `CMakeLists.txt:450,805` pin reactor ef7ce3b, which lacks `reactor_tls.h` and
  `connection_admission.h`. It is 14 commits before 56783e5.
- `CMakeLists.txt:783` pins http 31a037c. It lacks `http_asio_tls.h`,
  `TimedConnectionFactory`, the connection bound, and `set_request_timeout`. It is 17
  commits behind HEAD, including 708f457, 9e3478c, 8ff1841, af36648, and 8f27b3c.
- The local qualification override points reactor, http, and cluster at the sibling
  checkouts (`.scratch/local-dependencies.cmake:39,41-42`). A local build sees code that
  CI and releases do not.
- OpenSSL appears nowhere in the build or packaging:
  - CMake has no match.
  - CI installs no OpenSSL package (`.github/workflows/ci.yml:31-33,112-114`).
  - The Docker runtime stage lists none (`Dockerfile:52-59`).
  - The Homebrew formula has no `depends_on` for it (`contrib/homebrew/xapiand.rb:23-28`).
  - The FreeBSD job installs none (`.github/workflows/freebsd.yml:38`).

Why it matters: "the one leg the libraries already carry" (design:20) holds at library
HEAD, not at Xapiand's pins. The bump brings 31 commits (14 reactor, 17 http), among them
deferred replies, owned streaming, connection admission, and accept recovery, before any
TLS code. Making OpenSSL a dependency changes every package.

Required change:

1. A first step that bumps reactor and http and passes the existing layers in CI on the
   new pins, plain, before any TLS code: smoke, unit, e2e, cluster, recovery.
2. Name the OpenSSL 3 dependency.
3. Say whether TLS is a CMake option, and its default.
4. List each packaging change.

#### TS-R20. D1.5 needs an http hook the design does not list (condition)

`serve_tls_connection` returns silently on every failure path
(`http/http_asio_tls.h:24,26-29,43-46,53,61-63`) and reports no reason. Only
`set_idle_timeout()` is named as an http addition (design:63). T2's "logged with its
reason" depends on this hook.

Condition: name the http addition (a failure observer that carries the reason), test it in
http, and rate-limit it in Xapiand.

#### TS-R21. Some gate-row cases cannot fail as written (condition)

- T2 and T6 refuse a static-RSA suite, but a server with an ECDSA certificate
  (`reactor/test/tls_test.cc:43`) can never negotiate one, with or without D1.4. Use an
  RSA certificate for the static-RSA case; the CBC case works with either key type.
- T2's TLS 1.1 attempt can fail inside the client: OpenSSL 3 clients do not offer TLS
  1.1 at the default security level. Lower the client's level, and assert the server's
  logged reason (TS-R20).
- T2's renegotiation case must force TLS 1.2.
- T2 expects startup to fail on expired and not-yet-valid certificates. No decision says
  so, and `reactor::tls::configure` checks no validity dates
  (`reactor/reactor_tls.h:67-71`). Decide it in D1.1. Under D1.6's restart-only rule, an
  expired certificate at restart takes the API down, so warn ahead of expiry.
- T3's "the 65th waits or is refused": http refuses it (`http/http_asio_tls.h:43-46`).
  Pin that outcome.
- T1's readiness probe needs the CA, and the test certificate's SANs must cover
  `localhost`.
- T1's newman flag: this Mac's newman 4.5.0 has no `--ssl-extra-ca-certs`
  (`newman_tls_options.out`), while CI installs the latest
  (`.github/workflows/ci.yml:76,129`). Use `NODE_EXTRA_CA_CERTS`, or require a newman
  version.
- T4 must say where the server's CPU time comes from (`/proc/<pid>/stat`, `ps`, or a
  server metric). Pinning the load generator needs `taskset` or `cpuset`; macOS has
  neither.
- T5: CI has no UBSan job (`.github/workflows/ci.yml:99-102`) and installs no OpenSSL
  development package. Add both.
- T6 also checks that no TLS 1.2 ticket is issued, and states TLS 1.3's behavior
  (TS-R22).

#### TS-R22. D1.4's ticket rule misstates TLS 1.3, and D1.4 is not additive (condition)

- `SSL_OP_NO_TICKET` is a server-side option. Under TLS 1.3 it switches to stateful
  tickets held in the server's session cache (`openssl_no_ticket_doc.out`, from OpenSSL
  3.6.5's `SSL_CTX_set_options`). So "TLS 1.3 keeps OpenSSL's defaults" (design:71-72)
  does not hold with that call. Name the calls (`SSL_OP_NO_TICKET`, plus
  `SSL_CTX_set_num_tickets` if TLS 1.3 tickets go too) and their effect.
- Restricting suites for every role removes options from consensus's transport and from
  Detent (design:72-73). That is a behavior change, not an addition. Run consensus's TLS
  tests and coordinate with Detent before landing it (TS-R17).

#### TS-R23. The certificate and key load late, after the privilege drop (condition)

The HTTP service, and so the key read, is built in `make_servers()` during node setup
(`src/manager.cc:769,910`). That runs after `demote()` drops privileges
(`src/main.cc:1091`) and after discovery starts. A key readable only by root fails, and a
bad key surfaces after the node has begun joining.

Condition: load and check the certificate and key before `demote()` and before discovery.
Refuse a key file that group or others can read (`Detent/docs/RELAY.md:15`).

#### TS-R24. Docs must say what HTTPS does not cover in a cluster (condition)

A write or query that arrives over HTTPS still crosses to other nodes in clear. Writes
forward to the primary over the remote protocol (`ARCHITECTURE.md:342-343`), and
distributed search and replication run over plain TCP. Step 1's threat model scopes node
traffic out (design:34), but users read docs, not designs.

Condition: the user-facing docs and the changelog entry say this, so HTTPS on the API is
not read as cluster security.

### What approval needs

1. TS-R18: D1.3 rewritten. Bounds apply only with TLS, or the change is declared
   breaking; header and idle bounds replace the exchange deadline; the connection cap is
   an explicit number; T3 matches.
2. TS-R19: the dependency step and the OpenSSL and packaging decisions are in the design.

### Conditions carried to the step-1 gate

1. TS-R3, TS-R14, TS-R17: the deferred table also carries a freshness rule for first
   contact after a receiver restart, the full threat-model rewrite, and the layering and
   Detent-coordination items.
2. TS-R20 to TS-R24 as stated above.

Not run (re-check): no build, no tests, no TLS handshakes. http's request-deadline
behavior is read from code, not exercised. newman was checked only on this Mac (4.5.0).
Nothing under Detent was touched.
