# Transport security

Status: design, for architect review before any code. Step 1 is opt-in and changes nothing
for existing deployments; step 2 makes it the default in a breaking release.

## Why

Xapiand listens on every interface by default (`--bind-address` is empty), and none of its
traffic is protected:

| Traffic | Today | Exposure on a shared network |
|---|---|---|
| REST API, TCP 8880 | Plain HTTP, no authentication | Anyone who reaches the port reads, writes, and deletes every index |
| Remote and replication protocols, TCP | Plain, no peer identity | Anyone who reaches a node reads its data, or poses as a node and serves it writes |
| Discovery, UDP multicast `239.192.168.1` | Plain, no authentication | Anyone on the segment reads membership, or joins the cluster |

Detent, built on the same libraries, requires TLS on every non-loopback listener
(`docs/AUTHORITY_TRANSPORT.md`, `docs/RELAY.md`): mutual TLS with pinned certificates
between peers, and server-authenticated TLS plus credentials for clients. Xapiand adopts
that rule (D6) once it can make the certificates itself (D5).

## Threat model

In scope:

1. A passive observer on the network between clients and nodes, or between nodes.
2. An active attacker on that network: tampering, replaying, impersonating a node or a
   client, joining the cluster through discovery.
3. A host on the network that can reach any port.

Out of scope here:

1. A compromised node. Its certificate is revoked by rotating the cluster authority (D5).
2. Authorization within the API: who may read or write which index. Step 2 adds an API
   token (D7), and per-index permissions are a later design.
3. Denial of service beyond what the existing connection caps and handshake deadlines
   bound.

## Decisions

**D1. HTTPS for the REST API.** `--tls-cert FILE` and `--tls-key FILE` serve the API over
TLS through http's `tls_connection_factory_timed` and `reactor::tls`'s Server role, which
needs no authority (reactor `56783e5`). `--tls-ca FILE` also verifies client certificates,
for mutual TLS from clients that have them. TLS 1.2 or later, no renegotiation, no early
data, a 64 KiB certificate-list cap, bounded handshakes: reactor's settings, unchanged.
The plain listener stays available only as D6 allows.

**D2. Mutual TLS between nodes.** The remote and replication protocols run over
`reactor::tls`'s Peer role: both ends present certificates signed by the cluster
authority (`--cluster-tls-cert`, `--cluster-tls-key`, `--cluster-tls-ca`). After the
chain, a node checks that the peer certificate's name is the node name discovery
announced for that address, so one node's certificate cannot serve as another's. A node
with cluster TLS refuses plain peers, and the reverse.

**D3. Authenticated, encrypted discovery.** Discovery cannot use TLS: it is UDP multicast,
and DTLS works only between two endpoints. The cluster library's `Bus` gains an optional
key (`--cluster-key FILE`, 32 random bytes): each datagram is sealed with
ChaCha20-Poly1305 through OpenSSL's EVP interface. The nonce is the sender's 64-bit
incarnation (random at start) and a 32-bit counter; the associated data is the cluster
name and the message type. A receiver drops datagrams that fail to open, and those whose
counter is not above the last seen for that sender's incarnation, within a 1,024-entry
window per sender. Without the key, a node cannot join, read membership, or replay an
announcement. The change lives in cluster's `Bus`, so Detent's legacy UDP profile can use
it too.

**D4. Mixed clusters are refused.** Every protection is all-or-nothing per cluster: a node
configured with D2 or D3 rejects peers without them, and logs why. A mixed cluster would
run partly in plain text and look healthy.

**D5. Certificates made on first start** (step 2; the `reactor::pki` helper, optional,
behind reactor's OpenSSL dependency):

1. **First node.** It creates a cluster authority (an ECDSA P-256 key and a self-signed CA
   certificate, ten years) and its own node certificate, in `<data>/.tls/` at mode 0600,
   and prints the authority's SHA-256 fingerprint.
2. **Enrollment.** `xapiand --enroll` on an existing node prints a one-time token: a random
   128-bit secret and the authority's fingerprint, valid for 30 minutes. The node keeps
   only the secret's hash and refuses it after one use.
3. **Joining.** A new node started with `--enrollment-token` connects to the node that
   issued it over TLS, checks the presented authority against the token's fingerprint
   before sending anything, and sends a certificate request with the secret. It receives
   its signed certificate and the discovery key. The authority's key never leaves the node
   that holds it.
4. **Lifetime.** Node certificates last 90 days and renew automatically at two thirds of
   their life, over the same mutual-TLS channel. A compromised node is answered by
   rotating the authority: the cluster runs both old and new for one renewal period, then
   drops the old one. No revocation list.
5. **Bring your own.** With D1/D2's flags given, nothing is generated.

The helper stays small: key generation, CA and leaf signing, request verification, and
token issue and redemption, over OpenSSL's documented calls. It is reviewed as security
code before Detent may adopt it.

**D6. TLS required off loopback** (step 2, Detent's rule). A listener bound to a loopback
address may be plain. Any other listener requires TLS, unless the operator passes
`--plaintext` (REST API) or `--insecure-cluster` (between nodes and discovery), each
logged as a warning at every start. With D5, the default start of a node needs no flags:
it generates what it needs.

**D7. An API token** (step 2). D6 protects traffic, not access: anyone who reaches the
API can still use it. A node generates an API token at first start (printed once, stored
hashed) and requires it as `Authorization: Bearer` on every request from a non-loopback
client; clients with a certificate signed by the cluster authority are accepted without
it. Per-index permissions are later work.

**D8. Certificate reload without restart.** `SIGHUP` reloads `--tls-cert` and `--tls-key`
into a new context for new connections; existing connections keep theirs. Renewal under
D5 uses the same path.

## Steps

1. **Opt-in** (a minor release): D1, D2, D3, D4, D8. Off unless their flags are given;
   existing deployments are unchanged.
2. **Secure by default** (a release marked breaking in the changelog): D5, D6, D7.

## Gate rows

| Row | Target | Procedure |
|---|---|---|
| T1 REST API over TLS | The docs E2E suite passes over `https://`, every assertion | `harness/run_all.sh e2e` against a node with `--tls-cert`/`--tls-key` |
| T2 Refusals | A client without the CA, a plain client to a TLS listener, a wrong-name peer, a peer from another authority, a datagram under the wrong key, a replayed datagram, and a mixed cluster: each refused and logged | A multi-node harness case per refusal |
| T3 Cluster | Three nodes with D2 and D3: discovery, replication, failover, and the cluster checks all pass | `harness/cluster_check.sh` with cluster TLS and the key |
| T4 Enrollment | A token works once, expires at 30 minutes, fails with a wrong fingerprint, and never moves the authority key | Unit tests for `reactor::pki`; a three-node join from tokens |
| T5 Cost | Query and indexing throughput over TLS within 5% of plain for the gate's workloads, handshakes excluded; handshake rate stated | The load layer, plain against TLS, interleaved |
| T6 Sanitizers | T1 to T4 clean under ASan and TSan | CI's sanitizer jobs |

## Open questions

1. Should the API token be per user from the start, rather than one per cluster?
2. Should D2 pin peers' leaf fingerprints, as Detent does, in addition to the name check?
   With D5's renewals, a pin set changes every 60 days, so a name bound to the authority
   seems the better fit.
3. Where should `--enroll` run when the authority's node is down: should the authority's
   key be replicated to a second node, at the cost of D5's "never leaves" rule?
