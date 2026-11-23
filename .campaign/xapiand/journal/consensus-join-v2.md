# consensus-join-v2: journal

State: working
Quest: .campaign/xapiand/quests/consensus-join-v2.md (the lead's; never edit it)

Code: consensus worktree `../consensus-wt/xapiand/consensus-join-v2`, branch
`quest/xapiand/consensus-join-v2`, base consensus:6e1122c.

## Doing

Step 1 (A1, A2): token v2 and the DTJN0002 exchange in `admission_join.h`, key pins
(`key_pin` in `tls.h`), the asynchronous issuer hook, `join` with a saved key, `Joined::save`
ordered so the certificate is the commit point, and the crash test.

## Next

1. Step 1 as above, with `admission_nodes_test` `issue`/`save` reworked and mutants.
2. Step 2 (A4): NodePins over key pins; the hello path re-checks chain and expiry; runtime
   reload of certificate and bundle closing links whose chain no longer verifies.
3. Step 3 (A3): the join exchange on the transport port (preamble `DTJN0002` before TLS, one
   peeked byte decides), its own pool (64, 8 per source), rate (10/s per source, delayed) and
   slot deadline (10 s), outside `handshakes`.
4. Step 4 (A5): hello DTHL0003 with boot and directory version, acceptor check through a hook,
   refusal reason in the refusing hello; mutant skip-boot-check.
5. Step 5 (A6): fuzzers fuzz/join_fuzz.cc (token v2, offer, request, answer) and
   fuzz/hello_fuzz.cc, 30 CPU-minutes each under ASan+UBSan.
6. Step 6 (A7): docs/mechanisms/admission.md, hello.md, host-connection.md.

## Blocked on

Nothing. msg-269 (links.h, connection.h and their tests) granted by the lead. msg-270 asks for
the NodePins users' pin helpers (seeds_test, rpc_hosts.h, groups_test, bench/transport_idle,
bench/flow_bench) for step 2. msg-274 asks Detent's lead about Peers' DER pins (F11).

## Contracts

- `consensus:join-protocol` changes (Detent's lead reviews any change to an API Detent calls):
  Detent calls Peers, `certificate_fingerprint` and `Fingerprint` only; none of them changes.
  NodePins pins become key pins (`key_pin`); Detent's Seeds wiring would compute pins with it.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <pending> |

## Result

<pending>

## Rulings I made

- Peers and session.h keep the DER certificate fingerprint (Detent's operator config pins it);
  NodePins alone moves to key pins. Peers goes with worker-pump-removal.
