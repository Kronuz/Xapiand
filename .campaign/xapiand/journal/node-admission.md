# node-admission: journal

State: waiting on jobs 261120-173336-84c9cf 261120-173338-bff111 261120-173341-5fae97 261120-173345-1b7fb0 261120-173348-7a0880 261120-173414-fedae6 261120-173352-e73591 261120-173354-32d648 261120-173357-a2fd86 261120-173400-6f1195 261120-173403-838770 261120-173405-0da59f; CI run 44850841777
Quest: .campaign/xapiand/quests/node-admission.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 2 (fresh contributor) at consensus:50446f8, main merged (consensus:6976aa0).
Round 1's reds: only 261120-105029-49a29d was a real red (no-pin-check survived refused_pins); the
other mutant jobs exit 8 = caught. CI 44797280480: refused_*/remove_* died of SIGPIPE on Linux.
- consensus:c669067 SIGPIPE fixed in the transport (R11); consensus:50446f8 no-pin-check fix (R12).
- suite (verify, A1,A2,A3,A5): 261120-173336-84c9cf
- mutants (`admission_mutants.py 50446f8 <name>`): no-pin-check consensus:7ea3a3a -> refused_pins
  261120-173338-bff111 (A1); no-repin-check consensus:49d4f5b -> refused_ca 261120-173341-5fae97
  (A1); remove-keeps-links consensus:be59279 -> remove_ca 261120-173345-1b7fb0 (A2); any-token
  consensus:9fd3607 -> issue 261120-173348-7a0880 (A5); send-signals consensus:d38487b ->
  connection_test peer_gone 261120-173414-fedae6 (A1,A2: the Linux SIGPIPE)
- plain at head: refused_pins 261120-173352-e73591, refused_ca 261120-173354-32d648 (A1),
  remove_ca 261120-173357-a2fd86 (A2), issue 261120-173400-6f1195 (A5), peer_gone 261120-173403-838770
- TSan, admission cases + connection_test: 261120-173405-0da59f (A2,A3,A5)

## Next

1. Read the jobs and CI 44850841777 (Linux must be green); fill Evidence; then State: ready for review.

## Blocked on

- File set (lead): the mechanism needs `include/consensus/transport/links.h` (routes, removal,
  the epoch check) and a minimal `host_transport.h` change (`admit`, `remove`, `call`'s route
  check, `nodes_` gone: +18/-4 lines; members `admit_node`/`remove_node`). Neither is in the quest's files; the lead's brief allowed
  host_transport.h minimally. Recommendation: add both to the file set. Detent's
  consensus-transport touches host_transport.h in other hunks (constructor checks, `store`); the
  merge is textual only near the constructor.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | `docs/mechanisms/admission.md` (consensus:674bb21) |
| A5 | pending |

Messages: the coordinator's D149 (default policy is the cluster CA): done in consensus:4bf833f —
`NodePins(local, pins, context)` builds `ClusterCa(context, local)`; `pre_approved_pins()` is the
alternative; the `ca` test cases run the default.
D150 (A5, issue on join): done in consensus:7d767c6 — `admission_join.h`: join token
(`DTJT`, CA fingerprint + 32-byte secret), `join()` (own P-256 key, CA pinned by the token before
the secret is sent), `Issuer` (constant-time secret check, `assign`/`issued` hooks, signs a
certificate under the CA policy's prefix), `JoinFault`; `issue` test case; mutant `any-token`.

## Result

consensus:64dbb06 on quest/xapiand/node-admission (not pushed). `NodePins` is a shared,
thread-safe directory with runtime `admit`/`remove`, a removal epoch and named refusals
(`PinFault`); the trust policy defaults to the cluster CA (D149, pin recorded at first join), with
`pre_approved_pins()` as the alternative; `HostTransport::admit_node`/`remove_node` route and drop
nodes on their loop (`links.h` routes, removal, epoch check); `admission_join.h` issues a joining
node's certificate for the join token (D150). Probe 261120-104658-5f4562: every admission case
passes; 132,010 calls between two nodes during 8 remove/re-admit cycles of a third, 0 lost; a
removal ends the removed node's calls both ways in 5-6 ms. How it works:
`docs/mechanisms/admission.md`. Open: a seed dial by address returning the node (follow-up);
serving the join exchange on the transport's own port (R8).

## Rulings I made

- R1: admission lives in `HostTransport`/`Links`/`NodePins` (what Xapiand uses, cluster B2), not in
  the per-group `Peers` (peers.h), which worker-pump-removal deletes; peers.h is untouched.
- R2: `NodePins` copies share one mutex-guarded directory, so the per-loop `Links` copies see one
  admission at once; the lock is per handshake and per link, never per frame. Pin lookup is a
  fingerprint map instead of a scan of every pin.
- R3: the cluster CA's identity is a subjectAltName URI `<prefix><decimal node id>`, the prefix
  derived from the local certificate's URI ending in `/<local id>` (no flag to disagree with the
  certificate). A local certificate naming no node leaves only pins admitting (`unpinned`), so
  every existing caller and fixture keeps its behavior and messages.
- R4: a removed node keeps a tombstone (and its fingerprint), so its certificates are refused as
  `removed` under either policy until `admit` again; `admit(node)` with no pin on a removed node
  lets the CA pin a re-keyed certificate at its next first join (B3's operator re-key).
- R5: the remove-versus-handshake race is closed with a removal epoch: `Links` records `epoch()`
  per connection before its handshake, and a connection becomes a link only while
  `current(node, since)`. Re-authenticating at reserve would need `connection.h` (outside scope).
- R6: `admit`/`remove` run on `loop_of_node(node)`, like `call`, so routes stay loop-local and
  need no lock; removal aborts (not drains) the links, since the node is no longer trusted.
- R7: refusals keep their old messages; new faults have their own (`PinRefused::fault`), counted
  in `NodePins::refused(fault)`.
- R8 (A5): the join exchange runs on a socket the caller accepts for it, not on the transport's
  listener, since the transport's handshake requires a client certificate a joiner lacks; serving
  it on the same port would change `connection.h`/`links.h` accept (outside scope). The issuer is a
  node holding the CA key; node ids come from the caller's `assign` hook.
- R10: the three deadline.h mutants of `mutant_commit.py` (no-extension, uncapped, twice) no
  longer apply at the quest's base e1f249d: pre-existing, outside this quest, left alone.
- R9: HostTransport's members are `admit_node`/`remove_node`: a member `admit` hides the
  namespace's per-frame `admit()` inside the class.
- R11 (lead's direction, round 2): the Linux SIGPIPE was the transport's: OpenSSL's socket BIO
  writes with write(). `Connection` now gives OpenSSL its own write BIO (`send(..., MSG_NOSIGNAL)`,
  `BIO_sock_should_retry`), reads stay on the socket BIO; `SO_NOSIGPIPE` goes (one mechanism on
  every platform). Outside the quest's file set, as the lead asked: connection.h,
  connection_test.cc (case `peer_gone`; no SIGPIPE ignore; its raw test writes use MSG_NOSIGNAL),
  links_test.cc (its ignore removed), host-connection.md. Not touched: main's seeds test still
  ignores SIGPIPE (consensus:8801b99) and seeds.md calls it the host's duty: now redundant, the
  lead's follow-up; rpc/tls/peers tests keep their ignores (tls_test's pipes need it).
- R12 (A1): `pre_approved_pins()` is an empty TrustPolicy, not a policy answering node 0; the
  unpinned refusal in `authenticate` is then the check it runs, and no-pin-check bites.
- Open, not in A1-A5: a seed dial by address that returns the authenticated node (`Seeds::on_dial`
  hands an address). With the CA default the joiner can learn a seed's node from its certificate;
  a follow-up quest (`Links::identify(address)`) is the recommendation.
