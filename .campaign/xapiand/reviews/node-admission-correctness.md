# node-admission review: correctness (carrying the security questions)

Commit: consensus:50446f8c835a60326a683fe6014cf63b396c9d85
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- Mutants killed at 50446f8 (`--fails-without`, exit 8 = ctest failed): no-pin-check
  261120-173338-bff111 (refused_pins), no-repin-check 261120-173341-5fae97 (refused_ca),
  remove-keeps-links 261120-173345-1b7fb0 (remove_ca), any-token 261120-173348-7a0880 (issue).
- Plain cases at 50446f8, exit 0: refused_pins 261120-173352-e73591, refused_ca
  261120-173354-32d648, remove_ca 261120-173357-a2fd86, issue 261120-173400-6f1195, connection
  peer_gone 261120-173403-838770 (macOS: "write 2 to the closed peer failed, with no SIGPIPE").
- CI 44850841777 at 50446f8: linux-tsan, linux-asan-ubsan, linux gcc/clang/arm64, freebsd and
  macos-x86_64 green. macos-arm64 was still running when I wrote this. The linux-tsan log shows
  every `admission_nodes_*` case and `consensus_transport_connection` passing under TSan. That is
  the A2 TSan evidence: the local TSan job 261120-173405-0da59f and the suite 261120-173336-84c9cf
  were cancelled, not failed.
- 261120-173414-fedae6 (send-signals) is not evidence: see N4.

## What I checked myself

Scripts under `.scratch/xapiand/review-node-admission-correctness/` in the Xapiand checkout. Rerun
them at the fix commit.

- `mutants.py` seeds one guard fault at a time into a copy of the checkout, rebuilds, and runs the
  cases that should catch it. The unmutated base passed in every job.
  - 261120-175504-67a2b6: links-no-epoch SURVIVED every admission case and links_test;
    owner-no-removed KILLED by remove_ca and remove_pins; first-join-no-removed SURVIVED.
  - 261120-175507-dc9db5: join-unchained-issuer SURVIVED issue; join-no-ca-fingerprint KILLED by
    issue; join-accepts-any-issued SURVIVED issue; ca-no-verify KILLED by policy.
  - 261120-175510-a2f2fe: remove-no-pending-fail SURVIVED remove_ca and remove_pins;
    remove-keeps-route KILLED by remove_*; bio-no-retry (no retry flag on EAGAIN) KILLED by
    connection.
- `probe.sh` with `join_probe.cc`, job 261120-175513-391496:
  - Impostor: a fake issuer presents a rogue-CA leaf (rogue3) and sends the token's real CA
    certificate. The joiner fails with `JoinRefused ca`, and the impostor receives 0 bytes.
  - Member: a fake issuer presents a cluster node's own leaf (n2) without the CA key. The joiner
    sends it the secret (247 bytes, secret present).
  - `Joined::save` into a directory it cannot write throws only from the key's chmod ("in
    permissions: No such file or directory"). With umask 022, a watcher saw `node.key` written and
    readable by group and other in 329,157 of 26,180,204 samples. The final mode is 600.
- I read `node_pins.h`, `admission.h`, `admission_join.h`, `links.h`, `host_transport.h`,
  `connection.h`, the test, `admission.md`, and asio's `socket_ops.ipp` and `reactor_tls.h`
  configure in the build tree.

Answers to the lead's questions:

1. **No pin or chain, no RPCs.** `Connection::handshake` runs `authenticate` before any hello or
   frame (connection.h:117-127). After `remove_node`:
   - Links are aborted (remove-keeps-links killed).
   - The route is erased (remove-keeps-route killed).
   - The removed node's certificate is refused by name (owner-no-removed killed).

   A dial or accept whose TLS passed before the removal is stopped by the epoch check at
   `reserve` and `adopt` (links.h:252, 371-378). The code is right, but nothing tests it (F1).
   There is no runtime policy change: the policy is fixed when `NodePins` is built. The new default
   (CA) is inert for today's callers, whose certificates carry no node URI
   (tls_certificates.cmake:15).
2. **Join token.**
   - The secret is compared with `CRYPTO_memcmp`; only its length leaks.
   - No message carries the secret.
   - The joiner checks the CA by fingerprint and checks that the issuer's TLS leaf chains to it
     before sending; TLS proves the issuer holds that leaf's key. The impostor probe confirms
     this, but no test pins it (F2).
   - Replay: the exchange runs inside TLS to a key-proven issuer, so a passive or active
     man-in-the-middle cannot replay a join message.
   - The token itself is reusable and has no expiry. Those are design items (addition 1, M4), not
     this quest's.
3. **Write BIO.**
   - It mirrors OpenSSL's `sock_write`: `BIO_sock_should_retry` sets the retry flag on EAGAIN or
     EINTR, and a partial `send` count goes back to OpenSSL (bio-no-retry is killed).
   - A TLS 1.3 KeyUpdate during `SSL_read` surfaces as WANT_WRITE, which `want()` already waits on
     (connection.h:199-213). That path is unchanged by the split BIOs.
   - Renegotiation is off (`SSL_OP_NO_RENEGOTIATION`, reactor_tls.h:289).
   - `SSL_shutdown` uses the same write BIO, on the same loop as the reader.
   - Nothing needs `SO_NOSIGPIPE`: asio sets it itself on every socket it opens or accepts on Apple
     and FreeBSD (socket_ops.ipp:126-129, 1899-1901), and `MSG_NOSIGNAL` covers Linux.
4. **Concurrency.** `admit` and `remove` are confined to the node's loop. The directory is
   mutex-guarded and `refused` is atomic. The policy runs outside the lock; `first_join`
   re-checks owner, removed and pin under the lock. TSan is green on Linux CI for every case,
   including `remove` under live 1-2 traffic. The epoch interleaving is the one the design names
   and no test forces (F1).
5. **Mutants.** See F1, F2 and N1-N3 below. Every other guard I seeded was killed.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | The removal epoch (R5), the only guard against a connection authenticated before `remove_node` becoming a link, has no test that fails without it: `Links::current` returning true passes every case. admission.md names this race as the one that remains. | 261120-175504-67a2b6 (links-no-epoch SURVIVED); links.h:252, 371-378 | Add a deterministic case that forces the interleaving under both policies: TLS completes (dial, and an accept moved from another loop), then `remove_node`, then the hello arrives. It must leave no link and no call answered. Add `links-no-epoch` to admission_mutants.py and tag it `--fails-without`. |
| F2 | yes | gap | The joiner's check that the issuer's own TLS leaf chains to the token's CA is untested. It is what stops a host that replays the public CA certificate from collecting the secret. The probe shows the check works today, but removing it passes `issue`. | 261120-175507-dc9db5 (join-unchained-issuer SURVIVED); 261120-175513-391496 (impostor: `ca`, 0 bytes) | Add the impostor to `issue`: a fake issuer with rogue3 sends the real CA certificate; expect `JoinFault::ca` and no request bytes received. Add the mutant to admission_mutants.py. |
| F3 | yes | defect | `Joined::save` writes `node.key` through `std::ofstream` (mode 0666 masked by umask) and then chmods it, so the private key is readable by group and other for a window. No write is checked: a failed `node.pem` or `ca.pem` write returns normally, and a failed key write surfaces only as the chmod's ENOENT. | 261120-175513-391496 (329,157 of 26,180,204 samples readable by group and other; unwritable directory threw only from `permissions`); `admission_join.h`, `Joined::save` | Create the key with `open(O_CREAT\|O_EXCL\|O_WRONLY\|O_NOFOLLOW, 0600)`, or as a 0600 temporary file renamed into place (addition 2's rename). Check every write and close and throw on failure. Test: a save into an unwritable directory throws naming `node.pem`, and `node.key` is created 0600 under umask 022. Keep the probe's watcher as a coverage report that never fails the run. |
| N1 | no | gap | Nothing tests the joiner's check of the issued certificate (its own key, chaining to the CA, a node URI). | 261120-175507-dc9db5 (join-accepts-any-issued SURVIVED) | Add a fake issuer that answers another key's certificate (`issued`), plus its mutant. |
| N2 | no | gap | `remove_node` failing the calls still waiting for a connection (`not_connected` at once) is untested: without it, those calls end only at their deadlines. | 261120-175510-a2f2fe (remove-no-pending-fail SURVIVED) | Make a call to an unreachable admitted node, `remove_node`, and expect `not_connected` well before the deadline. |
| N3 | no | gap | `first_join`'s `removed` refusal is untested: with it gone, the epoch check still refuses the link, but by no name. | 261120-175504-67a2b6 (first-join-no-removed SURVIVED) | Under CA: `remove_node` a node never joined, then dial with its CA certificate; expect `refused(removed)` to rise. |
| N4 | no | gap | The send-signals mutant cannot fail on macOS or FreeBSD, because asio sets `SO_NOSIGPIPE` on its sockets there. Its local job "failed" (the mutant survived) and proves nothing either way. Linux is the only platform where `MSG_NOSIGNAL` matters. | 261120-173414-fedae6; asio socket_ops.ipp:126-129, 1899-1901; CI 44797280480 (round 1: Linux SIGPIPE deaths without the fix) | Run the mutant on Linux (a CI push of the mutant commit, or a Linux VM) and tag that run, or record CI 44797280480 as the fails-without. Do not tag the macOS run. |
| N5 | no | nit | Any cluster member, meaning any holder of a CA-signed leaf without the CA key, can pose as an issuer and receive the join secret. This fits the trusted-cluster model and M4, where members relay joins, but admission.md does not say it. | 261120-175513-391496 (member n2 got the secret) | Add one line to admission.md: every member can read a token's secret. |
| N6 | no | nit | Under the default CA policy, trust is TOFU per host. An issued certificate is valid for a year and nothing revokes it. A removal's tombstone lives only on the host that removed the node. An `assign` hook that hands out an existing id lets the impostor pin first on hosts that have not met the real node. M5 (pins from the root directory) closes this for Xapiand. | node_pins.h `first_join`; admission_join.h `Issuer::issue` | Document the `assign` contract: never reissue an id. The rest is the cluster design's. |
| N7 | no | nit | `admit_node(local, address)` routes the local node; `remove_node(local)` throws. | host_transport.h:341-345 | Throw for the local node, as `remove_node` does. |
| N8 | no | nit | `Issuer` hooks run on whichever loop serves the join; nothing says they must be thread-safe when `serve` is spawned on several loops. | admission_join.h `Issuer::Hooks` | One comment line. |

## Not run

- macos-arm64 CI at 50446f8 had not finished when I wrote this.
- No local TSan run: I relied on CI linux-tsan.
- The check suite at 50446f8 locally (84c9cf was cancelled): I relied on CI.
- No Linux run of any mutant.
- No fuzzing of the DTJN0001 decoder with truncated or oversized frames: I read the bounds
  (64 KiB, length-prefixed, secret length checked before `get64`).
- I did not check Detent's adoption of the new `NodePins` default.
