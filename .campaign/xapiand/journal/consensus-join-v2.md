# consensus-join-v2: journal

State: working
Quest: .campaign/xapiand/quests/consensus-join-v2.md (the lead's; never edit it)

Code: consensus worktree `../consensus-wt/xapiand/consensus-join-v2`, branch
`quest/xapiand/consensus-join-v2`, base consensus:6e1122c.

## Doing

Steps 3-6 written, uncommitted, building: A3 (Links::accept peeks the first byte inside the
handshakes' pool; the join magic's 'D' goes to JoinService's pool: 64, 8 per source, 10/s per
source delayed, slot 10 s, answer 10 s; test `flood`), A5 (DTHL0003: boot, directory, reason;
LinkOptions::check_boot async at both ends, boot_refused at the dialer; HostTransport::boot();
test `boot`), A6 fuzzers (fuzz/token_fuzz.cc, join_fuzz.cc, hello_fuzz.cc, CONSENSUS_FUZZ), A7
docs (admission.md, hello.md, host-connection.md).

Done: step 1 (A1, A2) consensus:166327d; step 2 (A4) consensus:a921f4e.

## Next

1. Build, run admission_nodes (all cases), links, connection, node_hello, hello, seeds; commit.
2. Mutant jobs: skip-boot-check (A5, boot), join-no-cap / join-no-slot-deadline / join-no-rate
   (A3, flood). Fuzz jobs (A6): token, join, hello, 2 workers x 1000 s each, ASan+UBSan, built
   with /usr/local/opt/llvm/bin/clang++ -DCONSENSUS_FUZZ=ON.
3. Push the branch; `campaign verify consensus-join-v2` for A1-A5, A8 (CI).
4. Old step list for reference: Step 4 (A5): hello DTHL0003 with boot and directory version, acceptor check through a hook,
   refusal reason in the refusing hello; mutant skip-boot-check.
5. Step 5 (A6): fuzzers fuzz/join_fuzz.cc (token v2, offer, request, answer) and
   fuzz/hello_fuzz.cc, 30 CPU-minutes each under ASan+UBSan.
6. Step 6 (A7): docs/mechanisms/admission.md, hello.md, host-connection.md.

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
| A1 | <pending> |

Jobs submitted: A2 mutants save-in-place 261123-132818-a116ef, sign-each-time
261123-132820-5a61c4; A4 mutants pin-der 261123-133811-fee953, reload-keeps-links
261123-133816-735a28, reload-keeps-trust 261123-133820-3e888d.

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
