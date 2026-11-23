# consensus-join-v2: journal

State: waiting on jobs 261123-133816-735a28 261123-133820-3e888d 261123-150157-b0634d 261123-150202-29c750 261123-150206-ee4ab3 261123-150210-a56a49 261123-150215-ec3793 261123-150231-e1409c 261123-150234-473c5f 261123-150236-7aa566, and CI at consensus:45f2309
Quest: .campaign/xapiand/quests/consensus-join-v2.md (the lead's; never edit it)

Code: consensus worktree `../consensus-wt/xapiand/consensus-join-v2`, branch
`quest/xapiand/consensus-join-v2`, base consensus:6e1122c.

## Doing

All code for A1-A7 is committed and pushed: consensus:166327d (A1, A2), consensus:a921f4e (A4),
consensus:45f2309 (A3 join pool on the port, A5 DTHL0003 boot check, the HostTransport redial
fix, A6 fuzzers, A7 docs). Locally: the 50 targeted transport tests pass (admission_nodes every
case, links, connection, node_hello, hello, seeds, rpc_host); reload stressed 40 times 8 at once
with the redial fix, 0 failures (before it: 4 of 96). Fuzzers ran 15 s each locally, clean.

## Next

1. Read the jobs' results into Evidence; CI at consensus:45f2309 (`campaign verify
   consensus-join-v2`): the suite, asan and tsan checks.
2. groups_test (Blocked on, below): its pin helper fails every case until the one-line change.
3. A security review is the lead's to launch.

## Blocked on

- The lead (msg-270): test/transport/groups_test.cc's pin helper must call `key_pin` (one
  line); it is held by Detent's consensus-fetch-alongside-flake. Until then groups_test compiles
  but every case fails at NodePins construction ("the local certificate is not the local node's
  key"), so the suite check cannot pass. After that quest lands: merge consensus main, ask the
  lead for the file, make the one-line change.
- msg-269 answered: links.h, connection.h, links_test, connection_test, mutant_commit.py granted.
- msg-274: asked Detent's lead whether Peers moves to key pins (F11); default: it stays DER.

## Contracts

- `consensus:join-protocol` changes (Detent's lead reviews any change to an API Detent calls):
  Detent calls Peers, `certificate_fingerprint` and `Fingerprint` only; none of them changes.
  NodePins pins become key pins (`key_pin`); Detent's Seeds wiring would compute pins with it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | suite (CI at consensus:45f2309); `issue` case; mutants any-token, join-any-cluster, join-unpinned-ca, token-no-version |
| A2 | sign-each-time 261123-132820-5a61c4 (fails issue: 3 signatures), save-in-place 261123-132818-a116ef (fails save: the kill-during-save crash test) |
| A3 | `flood` case (suite, tsan in CI); join-no-cap 261123-150202-29c750, join-no-slot-deadline 261123-150206-ee4ab3, join-no-rate 261123-150210-a56a49 |
| A4 | `renew`, `expired`, `reload`, `redial` cases; pin-der 261123-133811-fee953 (done, fails renew), reload-keeps-links 261123-133816-735a28, reload-keeps-trust 261123-133820-3e888d, redial-at-deadline 261123-150215-ec3793 |
| A5 | `boot` case; skip-boot-check 261123-150157-b0634d |
| A6 | token 261123-150231-e1409c, join 261123-150234-473c5f, hello 261123-150236-7aa566 (2 workers x 16 min each, ASan+UBSan) |
| A7 | docs/mechanisms/admission.md, hello.md, host-connection.md at consensus:45f2309 |
| A8 | CI run 45285292077 at consensus:45f2309 (queued) |

## Result

<pending>

## Rulings I made

- Peers and session.h keep the DER certificate fingerprint (Detent's operator config pins it);
  NodePins alone moves to key pins. Peers goes with worker-pump-removal.
- Token v2 text: "DTJT0002" and hex of cluster (16), CA key pin (32), secret (32): 168
  characters. A "DTJT" and 128 hex digits token (v1's shape) or "DTJT" and four other digits
  is `version`; anything else not v2 `malformed`.
- The exchange's version is an 8-byte magic sent before TLS ("DTJN0002"): the acceptor peeks one
  byte to tell a joiner ('D') from a node (a TLS record, 0x16). No ALPN, no callback on the
  shared context.
- The member does not check the secret: the hook does (the root keeps only its SHA-256;
  `join_secret_digest`, `join_secret_matches`). The member checks the request's cluster and the
  CSR's signature. Hook refusals carry a reason string (expired, revoked, used, name) under
  JoinFault::refused; `node` was renamed `refused`.
- The offer names every CA the member's context trusts (up to 16), so a bundle mid-replacement
  works; the joiner picks the one whose key the token pins.
- Idempotency per key: the hook names the same node for the same key; Issuer signs once per
  (key, node) within a 10-minute retry window (in memory, mutex), so concurrent joins and a retry
  get one certificate. Past the window a retry gets a new certificate for the same key, the same
  pin.
- Joined::save order: node.key, ca.pem, node.pem; the certificate is the commit point.
- The request carries an opaque `detail` (name, index paths for M3) within the 64 KiB frame.
- A5 shape: DTHL0003 (180 bytes) adds boot (16), directory version (u64) and a refusal reason
  (u32) after the connection id. `LinkOptions::check_boot(peer hello, loop)` is a coroutine
  returning 0 or the caller's code; both ends run it (the acceptor before the tie-break, the
  dialer on the accepting hello), the acceptor's code goes back in `reason`, and the dialer calls
  `LinkOptions::boot_refused(peer, reason)`. `HostTransport::boot(id, directory)` changes them
  live. An acceptor without a check starts the link at its reply, and the dialer's check closes
  it.
- Pre-existing HostTransport bug fixed (host_transport.h, in the file set): a Bulk call made
  within `redial` of a failed dial, when that dial's failure saw no call waiting, never dialed
  until its deadline (pump returned on `dial_after` and armed no redial). Found as a 1-in-16
  flake of `reload` under load (no dial after the second reload; instrumented locally). Fix:
  pump arms the peer's redial for the spacing's end. Test `redial`, mutant `redial-at-deadline`.
