# Transport security

Status: revision 3, approved with conditions for step 1 (second re-check); this revision
folds the conditions C1 to C8 (TS-R25 to TS-R32) into the decisions and rows below. The review
(`docs/reviews/transport-security-design.md`) rejected revision 0 and re-checked revision 1:
revision 1 narrowed step 1 to HTTPS for the REST API; this revision answers the re-check's
TS-R18 (D1.3's deadline) and TS-R19 (the dependency step), and its conditions TS-R20 to
TS-R24. Everything else in revision 0 is deferred, with the review's findings as the later
design's requirements.

## Why

Xapiand listens on every interface by default (`--bind-address` is empty), and none of its
traffic is protected: the REST API on TCP 8880 is plain HTTP with no authentication, the
remote and replication protocols are plain TCP with no peer identity, and discovery is plain
UDP multicast. The node-to-node legs cannot be wrapped as revision 0 designed (TS-R1: the
remote client is the vendored Xapian `RemoteConnection` on raw fds, and replication writes
whole database files to the socket fd); strata's replication replaces those paths, and
their design follows it. The REST API is the leg the libraries already carry, and the one
clients and operators touch.

## Threat model for step 1

In scope: a passive or active attacker on the network between a client and a node, who reads
or tampers with REST traffic, or impersonates the node to a client that verifies its
certificate.

Out of scope for step 1:

1. **Access control.** Anyone who reaches the port can still use the API; the API token is
   deferred (TS-R9).
2. **Traffic between nodes, and discovery**, which stay in clear (deferred). Data a client
   sends over HTTPS travels between nodes unprotected; the user docs and the changelog say
   so (TS-R24).
3. **Clients that do not verify the certificate**: they get confidentiality against a
   passive observer only.
4. **Denial of service** beyond D1.3's bounds, which are residuals with numbers.
5. Revision 0's other actors (same-host processes, browsers, secrets at rest) belong to the
   deferred design's threat model, rewritten whole there (TS-R14).

## Step 0. The dependencies (TS-R19)

Before any TLS code, one change that passes every existing layer in CI with plain HTTP:

1. **Pins.** Every Kronuz library moves from its 2026-09-18 pin to its current head. Since
   2026-10-17 every Kronuz library pins its own dependencies, so the block of 14 transitive
   pins in `CMakeLists.txt` is removed, and a configure confirms each library checks out at
   the commit its consumers pin. reactor then has `reactor_tls.h` and connection admission;
   http has the TLS connection factory, the six-argument constructor, and
   `set_request_timeout`. The local `.scratch/local-dependencies.cmake` override is not used
   for this step's verification.
2. **OpenSSL.** A CMake option `TLS`, on by default, requires OpenSSL 3 (configure fails
   without it unless `-DTLS=OFF`) and links `reactor::tls`. Packaging: CI's Ubuntu jobs add
   `libssl-dev`; the Docker runtime image adds `libssl3`; the Homebrew formula depends on
   `openssl@3`; the FreeBSD job uses base OpenSSL 3 (FreeBSD 14). A build with `-DTLS=OFF`
   has no TLS flags at all.
3. **Verification.** The pinned build passes CI's existing jobs: build, unit, smoke, load,
   e2e, and the sanitizer jobs, unchanged in behavior.

## D1. HTTPS for the REST API

**D1.1 Flags.** `--tls-cert FILE` and `--tls-key FILE` (PEM, given together, or startup
fails) serve the API over TLS on `--port`, replacing plain HTTP there. Without them nothing
changes: no bound in D1.3 applies, and plain HTTP behaves as today. There is no `--tls-ca`:
no reactor role offers optional client verification (TS-R11).

**D1.2 Mechanism.** `reactor::tls::configure(context, options, Role::Server)` (reactor
`56783e5`: the Server role needs no authority), and the HTTP service built with http's
six-argument constructor: `tls_connection_factory_timed(context, deadline, handshakes,
connections)` and the same connection bound for the service (`http/http_asio.h:826-833`,
`http/http_asio_tls.h:85-96`). The handshake deadline, handshake cap, and connection cap are
http's.

**D1.3 Bounds, with TLS only** (TS-R18):

| Bound | Value | Owner | Residual |
|---|---|---|---|
| Handshake deadline | 5 s | http | A peer holds a slot up to 5 s without completing |
| Concurrent handshakes | 64 | http | About 13 stalled connections a second from one source hold every slot; no per-source limit |
| Connections | `--tls-max-connections`, default 1,024 (a number, never derived from `max_clients`, which goes negative when the file limit cannot be raised); startup fails when it exceeds the open-files limit less 64 (C4) | http's admission | Connections idle up to the idle timeout hold slots |
| Idle between requests | 75 s, from the end of the handshake or of a reply to the first byte of the next request (C4) | An http addition, `set_idle_timeout()` | — |
| Inactivity within a request | 60 s without a byte read of a body or written of a reply (C4) | An http addition, `set_inactivity_timeout()` | A body or reply that moves at least one byte a minute holds a connection; 1,024 such connections from one host take every slot, as with plain HTTP today |
| Request headers | 30 s, from the request's first byte to the end of its headers | An http addition, `set_header_timeout()` | A client trickling headers holds a connection that long |

No deadline bounds a request body, dispatch, or the reply: a RESTORE of any size, a dump,
and slow handlers run as they do over plain HTTP. `set_request_timeout()`, which bounds the
whole exchange from before the request's first byte (`http/http_asio.h:613-619, 653, 707,
725, 769`), is not used.

**D1.4 TLS settings.** reactor's `Options` gains `strict = false`, which Xapiand sets. With
it, a context restricts TLS 1.2 to ECDHE key exchange with AEAD ciphers (OpenSSL's default
list includes four static-RSA suites without forward secrecy, TS-R16), issues no session
tickets in either version (`SSL_OP_NO_TICKET` and `SSL_CTX_set_num_tickets(ctx, 0)`: with
the first alone, TLS 1.3 tickets become stateful, TS-R22), and turns off the server's
session-ID cache (`SSL_SESS_CACHE_OFF`), so TLS 1.2 clients cannot resume either (C7). A
reconnecting client does a full handshake in both versions; T4 measures that cost. Without `strict`, contexts are unchanged, so consensus's and
Detent's are not affected; adopting `strict` there is their decision, after their TLS tests
(proposed in Detent's ledger, `docs/DEPENDENCY_UPGRADES.md`, maintainer proposals).

**D1.5 Handshake-failure logging** (TS-R20). http's TLS factory returns silently on every
failure path; it gains an optional callback `on_handshake_failure(reason)`, with reasons
version, cipher, certificate, protocol (not TLS at all, such as plain HTTP), and deadline
(C5). Connections refused at the cap never reach the factory: reactor refuses them at accept
(`reactor/reactor.h:289`), and Xapiand reports reactor's `rejected` counter as the cap reason. Xapiand counts each reason and logs at most
once a minute per reason.

**D1.6 The key, before anything else** (TS-R23, C6). Xapiand builds the TLS context in
`main()`, before `demote()` (`src/main.cc:1091`) and so before `server()`, `setup()`, and
discovery, and keeps it for `make_servers()`. Startup fails when the key file is readable by
group or others, when the key does not match the certificate (reactor checks this), or when
the certificate is not yet valid or has expired (checked by Xapiand from the certificate's
notBefore and notAfter). A certificate within 14 days of expiry logs a warning, checked at startup and daily
after, and its seconds to expiry are a metric (C6).

**D1.7 Certificate changes need a restart** in step 1: http's factory holds one context for
its life (TS-R12).

## Deferred

Each returns as its own design, written against strata's replication and discovery, which
remove TS-R1's raw-fd paths. The review's findings are its requirements.

| Decision (revision 0) | Requirements carried |
|---|---|
| D2 Mutual TLS between nodes | TS-R1 (every I/O path), TS-R2 (the dialer checks the node it meant, one SAN profile, unique issuance), TS-R13 |
| D3 Authenticated discovery | TS-R3 (replay across receiver restarts: persisted incarnations, and a freshness rule for first contact after a restart), TS-R4 (version in the AD, refusal of plain frames, one nonce sequence across sending threads, size and flood rules) |
| D4 Refusing mixed clusters | TS-R13: no half-secured mode |
| D5 Generated certificates | TS-R5 (eviction, key ids), TS-R6 (no split on first start or upgrade), TS-R7 (the enrollment protocol), TS-R8 (escrow and recovery of the authority) |
| D6 TLS off loopback | TS-R10 (classify by every listener's real address, discovery included) |
| D7 An API token | TS-R9 (no loopback exemption; token format, storage, constant-time comparison) |
| D8 Reload without restart | TS-R12 (a factory that swaps contexts, trust bundles, revoked sessions) |
| The threat model | TS-R14: rewritten whole, with every actor revision 0 omitted |
| Layering and coordination | TS-R17: which layer owns each mechanism, and the cluster `Bus` change coordinated with Detent |
| Other TCP services' bounds | TS-R16: idle and header timeouts and per-listener caps for the remote and replication services |

## Gate rows for step 1

| Row | Target | Procedure |
|---|---|---|
| T0 Dependencies | Step 0's build passes every existing CI job unchanged; a configure checks out every Kronuz library at its pinned commit, and every library's consumers pin the same commit of it (C2); the cluster checks with 2 and 3 nodes, WAL recovery, graceful shutdown, and signals pass on Linux and macOS without the local override, against a list of each behavior-changing commit range in the moved libraries and the check covering it (C1); the Docker image, the FreeBSD package, and the Homebrew bottle build without publishing, each stating the OpenSSL it links (C3) | CI on the step-0 commit; the configure's checkout check and `pin_consistency.py`; `harness/run_all.sh cluster recovery` on both platforms; the release workflows run with publishing off |
| T1 The API over TLS | The docs E2E suite passes over `https://`, every assertion, with the same assertion count as over plain HTTP | `harness/e2e_capture.sh` gains `XAPIAND_FLAGS` (passed to the node), `SCHEME` (the readiness probe and the collection base URL; the probe passes `--cacert`), and `CA` (newman's `--ssl-extra-ca-certs`, so newman 6 or later, which the harness checks), with a test certificate whose SAN covers `localhost` and `127.0.0.1` (C8); `docs_to_postman.py` takes the base URL from the environment. Plain and TLS from one build. |
| T2 Refusals, each beside an accept case differing in one variable, each asserting the server's logged reason | A client not trusting the CA (accepted: one that does); plain HTTP to the TLS port, logged as protocol (accepted: HTTPS); a TLS 1.1 ClientHello sent by a raw-socket client, so the refusal is the server's (accepted: TLS 1.2); a static-RSA TLS 1.2 suite against an RSA certificate (accepted: ECDHE-RSA-AES128-GCM); a renegotiation attempt over TLS 1.2, forced (C8); startup with an expired, a not-yet-valid, a mismatched, and a group-readable key or certificate (each fails, naming the reason; accepted: valid, mode 0600); `--tls-cert` without `--tls-key` | A harness script with `openssl s_client`, `curl`, and a raw-socket ClientHello |
| T3 Bounds | 65 connections stalled mid-handshake: the 65th refused, the 64 dropped at 5 s; a connection idle 75 s after its handshake, and one 75 s after a reply, closed; a body that stops for 60 s, and a client that stops reading a reply for 60 s, closed; `--tls-max-connections` above the open-files limit refused at startup; headers trickled past 30 s closed; a body streamed for longer than 120 s (a RESTORE) completes; at `--tls-max-connections`, the next connection refused; RSS per TLS connection at the cap stated | A harness script with raw sockets and `openssl s_client`, wall time |
| T4 Cost | Server CPU seconds per 1,000 requests, TLS against plain, within 5% for a keep-alive search loop and the docs E2E mix, interleaved, five trials, spread stated; full handshakes per second at one core stated | `harness/loadtest.py` gains HTTPS (a CA option), run from a separate process pinned away from the server's cores; the server's CPU seconds read from `/proc/<pid>/stat` on Linux and `ps -o cputime` on macOS before and after each trial (C8) |
| T5 Sanitizers | T1 and T2 clean under ASan, UBSan, and TSan | CI's sanitizer jobs, which run e2e today, gain `libssl-dev`, T1's TLS variant, and T2; the ASan job adds UBSan with `UBSAN_OPTIONS=halt_on_error=1`, so a report fails the job (C8) |
| T6 reactor's `strict` option | With `strict`: a static-RSA and a CBC TLS 1.2 suite refused, ECDHE-AEAD accepted, no tickets in TLS 1.2 or 1.3; without it, the negotiated suites and tickets unchanged | `reactor/test/tls_test.cc` over a memory BIO pair, with an RSA and an ECDSA certificate |
| T7 The http additions | `set_idle_timeout`, `set_header_timeout`, and `on_handshake_failure` each fire at their bound and reason, and change nothing when unset | http's `asio_test` |

## Answers to the review's open questions

1. Per-user tokens: not in step 1; the deferred D7 starts from the review's answer, a
   cluster-wide token set with ids, rotated by add-then-remove.
2. Leaf pins: no; the deferred D2 binds authority, cluster, and node name in the SAN, with
   uniqueness at issuance.
3. Authority down: the deferred D5 starts from escrow and restore, with an offline root and
   online issuing intermediates as an option, never an automatic copy.
