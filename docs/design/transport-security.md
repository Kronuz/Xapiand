# Transport security

Status: revision 1, for the architect's re-check. The first review
(`docs/reviews/transport-security-design.md`) rejected revision 0, approving D1's
direction. This revision narrows step 1 to D1 alone, HTTPS for the REST API, and defers
every other decision to a later design written against strata's replication, with the
review's findings carried as its requirements.

## Why

Xapiand listens on every interface by default (`--bind-address` is empty), and none of its
traffic is protected. The REST API on TCP 8880 is plain HTTP with no authentication, the
remote and replication protocols are plain TCP with no peer identity, and discovery is
plain UDP multicast. Revision 0 tried to protect all three at once; the review showed
that the node-to-node legs cannot be wrapped as designed (TS-R1: the remote client is
the vendored Xapian `RemoteConnection` on raw fds, and replication writes whole database
files to the socket fd), and that discovery's and enrollment's protocols needed more than
a decision each (TS-R3 to TS-R8).

The REST API is the one leg the libraries already carry: http's TLS connection factory
over `reactor::tls`'s Server role. It is also the leg clients and operators touch.

## Threat model for step 1

In scope: a passive or active attacker on the network between a client and a node, who
reads or tampers with REST traffic, or impersonates the node to a client that verifies
its certificate.

Out of scope for step 1, and why:

1. **Access control.** TLS protects traffic, not access: anyone who reaches the port can
   still use the API. An API token (revision 0's D7) is deferred with its review findings
   (TS-R9).
2. **Traffic between nodes and discovery.** Deferred (D2 to D4 below).
3. **Clients that do not verify the certificate.** They get confidentiality against a
   passive observer only; the operator distributes the CA or uses a public certificate.
4. **Denial of service** beyond the bounds stated in D1.3, which are residuals with
   numbers, not protections.

## D1. HTTPS for the REST API

**D1.1 Flags.** `--tls-cert FILE` and `--tls-key FILE` (PEM, given together, or startup
fails) serve the API over TLS on `--port`, replacing plain HTTP there: one listener, one
protocol. Without them nothing changes. There is no `--tls-ca` in step 1: no reactor role
offers optional client verification (TS-R11), and client certificates have no use until
an access model exists.

**D1.2 Mechanism.** `reactor::tls::configure(context, {cert, key, ""}, Role::Server)`
(reactor `56783e5`: the Server role needs no authority), and the HTTP service built with
http's six-argument constructor: `tls_connection_factory_timed(context, deadline,
handshakes, connections)` and the same connection bound for the service
(`http/http_asio.h:826-833`, `http/http_asio_tls.h:85-96`). The handshake deadline,
handshake cap, and connection cap are http's.

**D1.3 Bounds**, applied to the HTTP service with or without TLS:

| Bound | Value | Owner | Residual |
|---|---|---|---|
| Handshake deadline | 5 s | http | A peer holds a slot up to 5 s without completing |
| Concurrent handshakes | 64 | http | About 13 stalled connections a second from one source hold every slot; no per-source limit |
| Connections | `--max-clients` (`opts.max_clients`, computed in `src/main.cc` today and applied to no service) | http's admission | Idle authenticated connections hold slots until the idle timeout |
| Request deadline | 60 s per request, headers and body (`set_request_timeout`) | http | A slow client holds one connection that long |
| Idle keep-alive | 75 s between requests | An http addition: `set_idle_timeout()`, beside `set_request_timeout()`, which bounds only a request's own headers and body; tested in http's `asio_test` | — |

A TLS context's memory per connection at the cap is measured in T3, not assumed.

**D1.4 TLS settings**, in `reactor::tls::configure` for every role: TLS 1.2 or later,
no renegotiation, no early data, a 64 KiB certificate-list cap (already there), plus:
TLS 1.2 restricted to ECDHE key exchange with AEAD ciphers (OpenSSL's default list
includes four static-RSA suites without forward secrecy, TS-R16), and TLS 1.2 session
tickets disabled, so a ticket key never outlives the context it encrypts. TLS 1.3 keeps
OpenSSL's defaults. This is a reactor change, additive for every consumer: Detent's
contexts get the same restriction.

**D1.5 Logging.** A failed handshake is counted per reason (version, cipher, certificate,
deadline, cap) and logged at most once a minute per reason, so a scan cannot flood the
log.

**D1.6 Certificate changes need a restart in step 1.** http's factory holds one context
for its life (TS-R12); reload without restart arrives with the factory change it needs.

## Deferred

Each of these returns as its own design, written against strata's replication and
discovery once strata replaces Xapian's remote and replication protocols, which removes
TS-R1's raw-fd paths. The review's findings are its requirements.

| Decision (revision 0) | Requirements carried |
|---|---|
| D2 Mutual TLS between nodes | TS-R1 (every I/O path), TS-R2 (the dialer checks the node it meant, one SAN profile, unique issuance), TS-R13 |
| D3 Authenticated discovery | TS-R3 (replay across receiver restarts: persisted incarnations), TS-R4 (version in the AD, refusal of plain frames, one nonce sequence across sending threads, size and flood rules), TS-R14 (what stays in clear) |
| D4 Refusing mixed clusters | TS-R13: no half-secured mode |
| D5 Generated certificates | TS-R5 (eviction, key ids), TS-R6 (no split on first start or upgrade), TS-R7 (the enrollment protocol), TS-R8 (escrow and recovery of the authority) |
| D6 TLS off loopback | TS-R10 (classify by every listener's real address, discovery included) |
| D7 An API token | TS-R9 (no loopback exemption; token format, storage, constant-time comparison) |
| D8 Reload without restart | TS-R12 (a factory that swaps contexts, trust bundles, revoked sessions) |
| Other TCP services' bounds | TS-R16: idle and request timeouts and per-listener caps for the remote and replication services |

## Gate rows for step 1

| Row | Target | Procedure |
|---|---|---|
| T1 The API over TLS | The docs E2E suite passes over `https://`, every assertion, with the same counts as over plain HTTP | `harness/e2e_capture.sh` gains `XAPIAND_FLAGS` (passed to the node), `SCHEME` (readiness probe and collection base URL), and `CA` (newman's `--ssl-extra-ca-certs`); `docs_to_postman.py` takes the base URL from the environment. Run plain and TLS from one build. |
| T2 Refusals, each beside an accept case that differs in one variable | Refused and logged with its reason: a client not trusting the CA (accepted: one that does); plain HTTP to the TLS port (accepted: HTTPS); TLS 1.1 (accepted: 1.2 ECDHE-AEAD); a static-RSA TLS 1.2 suite (accepted: ECDHE); a renegotiation attempt; an expired and a not-yet-valid server certificate at startup (startup fails; accepted: valid); `--tls-cert` without `--tls-key` (startup fails) | A harness script with `openssl s_client` and `curl`, asserting each outcome and the log line |
| T3 Bounds | 65 connections stalled mid-handshake: the 65th waits or is refused, and the 64 are dropped at 5 s; idle connections closed at 75 s; at `max_clients` connections, the next is refused; RSS per TLS connection at the cap stated | A harness script with raw sockets and `openssl s_client`, timing with an injectable clock where the library allows, wall time otherwise |
| T4 Cost | Server CPU seconds per 1,000 requests, TLS against plain, within 5% for the docs E2E mix and for a keep-alive search loop, interleaved, five trials, spread stated; handshakes per second at one core stated | `harness/loadtest.py` gains HTTPS (a CA option) and is run from a separate process pinned away from the server's cores; CPU from the server's `getrusage` |
| T5 Sanitizers | T1 and T2 clean under ASan, UBSan, and TSan | CI's sanitizer jobs run the e2e layer today; T1's TLS variant joins them, and T2 runs in the same jobs |
| T6 reactor's TLS 1.2 restriction | A static-RSA suite and a CBC suite refused, ECDHE-AEAD accepted, for every role; existing contexts unchanged otherwise | `reactor/test/tls_test.cc` over a memory BIO pair |

## Answers to the review's open questions

1. Per-user tokens: not in step 1 (D7 is deferred). The deferred D7 starts from the
   review's answer: a cluster-wide token set with ids, rotated by add-then-remove.
2. Leaf pins: no; the deferred D2 binds authority, cluster, and node name in the SAN, with
   uniqueness at issuance, as the review answers.
3. Authority down: the deferred D5 starts from escrow and restore, with an offline root
   and online issuing intermediates as an option, never an automatic copy.
