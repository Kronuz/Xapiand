# consensus-join-v2: journal

State: waiting on jobs 261124-052541-de01a8 261124-052543-c2eb2f 261124-052545-d8b64d, and CI at consensus:d42a7d1
Quest: .campaign/xapiand/quests/consensus-join-v2.md (the lead's; never edit it)

Code: consensus worktree `../consensus-wt/xapiand/consensus-join-v2`, branch
`quest/xapiand/consensus-join-v2`, base consensus:6e1122c.

## Doing

2026-11-24, boot relink (CI 45391304329 at consensus:7e12650, linux-asan-ubsan:
`boot` "as the admitted boot, node 2 links and calls" FAILED, whole test 1.68 s; the other
failure there, `consensus_liveness` Timeout, is detent:consensus-liveness-asan-timeout's).
Cause, from the code: node 2 (acceptor, no check of its own) starts a link at its reply
(links.h adopt: reserve, write_hello, start). Node 1's last dial of the refusal phase was in
flight across `two.transport->boot(B)`: its reply hello carried C, node 2 had started the link,
`two.echoes(1)` bound its call there (host_transport.h call: `peer.lane` exists), node 1's
dialer check then refused C and aborted, and the call ended `connection_aborted` at once
(rpc.h Lane::close). Ruling: the test raced; the product keeps its contract (below). Fix at
consensus:d42a7d1 (test + hello.md only): node 1's check holds a dial on boot C (a gate) until
node 2 has switched to B and bound a call to the link it started; that call must end fast
(`connection_aborted`/`not_processed`, under 2 s of a 6 s deadline), node 1's redial must link
to B, then the original one-shot echoes run on the settled link. The old claim at the forced
interleaving: probe consensus:6719b63 (ref refs/probes/boot-old-claim, on d42a7d1; expects that
call `ok`) must fail. Jobs: 261124-052541-de01a8 (boot x10, Debug), 261124-052543-c2eb2f
(probe, fails-without), 261124-052545-d8b64d (boot x30 under ASan+UBSan, llvm clang).

What it touches, for the security reviewer's call: test/transport/admission_nodes_test.cc
(boot_case only) and docs/mechanisms/hello.md (one paragraph, one test line). No header, no
product code, no wire change. Evidence to rerun at the new head: CI (all platforms); the A5
mutant skip-boot-check (cherry-pick; it still fails `boot` in phase 1); the other six mutants
touch other cases and need only the rebuild. Groups (A1), forged/issue (S1) and the fuzzers
(A6) cover files this commit did not change.

msg-291 answered: the lead granted test/transport/groups_test.cc. Fix committed and pushed at
consensus:f01607b (`pin_of` returns `key_pin(leaf.get())`; test-only, no mechanism changes).
Before: 261124-011858-ff586e at 45bca9f exit 8, every groups case aborts with "node pins: the
local certificate is not the local node's key". The in-job probe 261124-011903-b4f548 was
cancelled (superseded). At the fix: groups cases 261124-013122-ade4a2 (A1); `campaign verify
--for A1` queued suite, asan, tsan (CI at f01607b); A6 fuzzers rerun at f01607b:
261124-013129-65e51c (token), 261124-013129-d849ad (join), 261124-013130-531f9c (hello).

2026-11-24, CI run 45290843170 at consensus:45bca9f failed on every platform: every
consensus_transport_groups_* case aborts at NodePins construction ("the local certificate is not
the local node's key"). Cause: test/transport/groups_test.cc:90 `pin_of` returns
`certificate_fingerprint` (DER); NodePins takes key pins since consensus:a921f4e and checks its
own (node_pins.h:187). Local jobs passed because no job ran groups_test (only admission_nodes,
links, connection, node_hello, hello, seeds, rpc_host). Fix: one line in the fixture,
`return key_pin(leaf.get());`; no non-test code changes. groups_test.cc is outside the file set
and held by detent:consensus-fetch-alongside-flake (its hunks miss line 90): msg-291 asks the
lead for it. Merged consensus main at consensus:963fd9a (clean), pushed. Jobs:
261124-011858-ff586e (A1 fails-without, groups cases at 45bca9f), 261124-011903-b4f548 (probe,
not evidence: groups cases at 963fd9a with the line patched in-job).


All code for A1-A7 is committed and pushed: consensus:166327d (A1, A2), consensus:a921f4e (A4),
consensus:45f2309 (A3 join pool on the port, A5 DTHL0003 boot check, the HostTransport redial
fix, A6 fuzzers, A7 docs). Locally: the 50 targeted transport tests pass (admission_nodes every
case, links, connection, node_hello, hello, seeds, rpc_host); reload stressed 40 times 8 at once
with the redial fix, 0 failures (before it: 4 of 96). Fuzzers ran 15 s each locally, clean.

Security review S1 (blocker, lead ruling 2026-11-24) fixed at consensus:45bca9f: `csr_of`
refuses a CSR whose key is not EC P-256 before any signature work (member and issuer);
`Issuer::issue` runs `assign` (the secret) before verifying; `verified()` counters. Test
`forged` (forged RSA-16384 CSR: refused malformed, 0 verifications; csr_of 0.04 ms locally).
Jobs: mutants csr-any-key 261123-154007-7bb36d, issue-verify-first 261123-154010-cd7dc9 (both
must fail `forged`); the reviewer's probe csr_cost at the fix 261123-154013-225d0b; forged and
issue at the fix 261123-154016-cd7eae. S2-S5 are the lead's (not fixed).

## Next

1. Read the jobs' results into Evidence; CI at consensus:45f2309 (`campaign verify
   consensus-join-v2`): the suite, asan and tsan checks.
2. groups_test (Blocked on, below): its pin helper fails every case until the one-line change.
3. A security review is the lead's to launch.

## Blocked on

- msg-291: resolved (groups_test.cc granted, fixed at consensus:f01607b).

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
| A1 | suite (CI at consensus:f01607b); groups cases 261124-013122-ade4a2, fails without 261124-011858-ff586e; `issue` case; mutants any-token, join-any-cluster, join-unpinned-ca, token-no-version |
| A2 | sign-each-time 261123-132820-5a61c4 (fails issue: 3 signatures), save-in-place 261123-132818-a116ef (fails save: the kill-during-save crash test) |
| A3 | `forged` case (S1) 261123-154016-cd7eae, csr-any-key 261123-154007-7bb36d, issue-verify-first 261123-154010-cd7dc9, probe 261123-154013-225d0b; `flood` case (suite, tsan in CI); join-no-cap 261123-150202-29c750, join-no-slot-deadline 261123-150206-ee4ab3, join-no-rate 261123-150210-a56a49 |
| A4 | `renew`, `expired`, `reload`, `redial` cases; pin-der 261123-133811-fee953 (done, fails renew), reload-keeps-links 261123-133816-735a28, reload-keeps-trust 261123-133820-3e888d, redial-at-deadline 261123-150215-ec3793 |
| A5 | `boot` case; skip-boot-check 261123-150157-b0634d |
| A6 | at consensus:f01607b: token 261124-013129-65e51c, join 261124-013129-d849ad, hello 261124-013130-531f9c (2 workers x 16 min each, ASan+UBSan) |
| A7 | docs/mechanisms/admission.md, hello.md, host-connection.md at consensus:45f2309 |
| A8 | asan, tsan and CI at consensus:f01607b (pending) |

## Result

<pending>

## Rulings I made

- Boot relink (CI 45391304329): the fast failure is the contract. A call is bound to its
  connection and ends with it, at most once (rpc-lane.md: `connection_aborted` on close,
  `not_processed` if it never left); the transport never sends it again, since only the caller
  knows whether it is safe to repeat. A dialer that refuses the acceptor's boot aborts a link the
  acceptor already started (two-message hello), and that window is inherent: closing it needs a
  third hello message, a wire change outside this quest. It hits only the refused (stale) boot's
  own calls, never the admitted node's: the dialer refuses before it starts. Test fixed, not the
  transport; hello.md states the window.
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
