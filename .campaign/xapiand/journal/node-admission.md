# node-admission: journal

State: waiting on reruns at consensus:781f0d0 (main merged; main touched host_transport.h): 261121-092255-2df121 0e0f26 02c927 b62830 b1a3bb 9b252d 57ecdd(TSan) 38cf24 210ae6, and CI. Then the lead: `campaign accept node-admission correctness --confirm F1=261121-092258-38cf24 --confirm F2=261121-092256-02c927 --confirm F3=261121-092259-210ae6 --fix-scope REASON` (since the review: open_of in the test; hold()'s nodes_ lookup swapped for routed()), then land
Quest: .campaign/xapiand/quests/node-admission.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix after the merge, consensus:9e66e5b, pushed. At 2374e2c every case building a HostTransport
aborted: main's constructor throws "member_of and open_of are required" and the admission hosts set
only member_of (epoch, save and policy build none, so they passed). The hosts now give a fixed,
non-empty open_of (they store no group). Unrelated to the served-slot fix or hold().
- Reruns at 9e66e5b: every case 261121-084542-caa3bb, issue 261121-084542-5d6e7b, refused_pins
  261121-084542-1a47bc, refused_ca 261121-084543-0d22ac, remove_ca 261121-084543-3e2e91, TSan
  261121-084543-9810bb, epoch 261121-084552-dda508, save 261121-084553-56b4a8. Transport set
  261121-062203-ed682b passed at 2374e2c (the test file is not its input).

Merge of main (detent:consensus-transport, consensus:9bbd632) at consensus:2374e2c, pushed.
- Conflict: connection_test.cc's case table; kept main's `dead_extends` and this branch's `peer_gone`.
- Semantic: main's new `HostTransport::hold()` checked `nodes_`, which this quest removed; it now
  asks `links->routed(node)` like `call()` (host_transport.h, still outside the file set).
- Reruns at 2374e2c: every admission case 261121-062150-a1f919, epoch 261121-062150-717a6c, issue
  261121-062150-1ef5dd, save 261121-062151-49e6e9, refused_pins 261121-062151-12a02d, refused_ca
  261121-062151-c8d423, remove_ca 261121-062152-b66750, TSan admission 261121-062152-cc45be; new:
  connection/links/groups/rpc_host 261121-062203-ed682b. Mutant jobs stand (their commits unchanged).


Done: fix round 1 of the correctness review (.campaign/xapiand/reviews/node-admission-correctness.md,
CHANGES at consensus:50446f8, blocking F1-F3), at consensus:803442f.
- F1 (consensus:0dce2e1, 803442f): case `epoch` (ca, pins) on loops the test steps: node 1's dial
  finishes TLS (`Links::connect`), node 3 is removed, then `offer`; and node 1 accepts on loop a,
  the move to its owner loop b is posted (b's `drained()` turns false), node 3 is removed, then b
  reads the hello. Neither becomes a link. Mutant `links-no-epoch` (the reviewer's: `Links::current`
  returns true). 803442f runs `stop()`/`remove()` on their loops: with the mutant's live link,
  `stop()` off the loop asserted in `asio_scheduler::cancel` and hid the accept half.
- F2 (consensus:0dce2e1): `issue` adds an impostor with rogue3's leaf replaying the token's CA
  certificate: the joiner refuses (`ca`), the impostor receives 0 bytes. Mutant
  `join-unchained-issuer`; at 0dce2e1 it failed `issue` with the impostor receiving 252 bytes.
- F3 (consensus:0dce2e1): `Joined::save` writes each file to `<name>.new` with
  open(O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC), the key 0600, checks every write (EINTR retried),
  fsync, close and rename, then fsyncs the directory; failures throw `std::system_error` naming
  the file and remove the staged file. Case `save`: modes under umask 022 (600/644/644); a forked
  child stopped by SIGXFSZ (RLIMIT_FSIZE 1024) at the key's write leaves only owner-only key bytes;
  EFBIG with SIGXFSZ ignored throws naming node.key and leaves none; an unwritable directory throws
  naming node.pem (skipped as root). Mutant `save-chmod-after` (the old ofstream + chmod writer);
  at 0dce2e1 it failed the mode check (node.key 644), the write check and the directory check.
- Jobs at 803442f: plain epoch 261121-113224-8358a1, issue 261121-113224-3a60bf, save
  261121-113225-813081; fails-without: links-no-epoch consensus:b58171c -> 261120-190024-386b01,
  join-unchained-issuer consensus:f8c0177 -> 261120-190026-ce7445, save-chmod-after
  consensus:1e2ab13 -> 261120-190027-3570be. Reruns at head: refused_pins 261121-112337-27fd23,
  refused_ca 261121-095253-b8cc25, remove_ca 261121-095252-330d78; every admission case
  261121-131011-10a70a. TSan and the suite: CI at 803442f (the local TSan rerun cancelled: CI's).
- Non-blocking, left for follow-up: N1 (issued-certificate check untested), N2 (remove_node's
  pending `not_connected` untested), N3 (first_join `removed` untested), N4 (send-signals mutant
  only meaningful on Linux: CI 44797280480), N5 (admission.md: every member can read a token's
  secret), N6 (document `assign` never reissues an id), N7 (`admit_node(local)` should throw),
  N8 (Issuer hooks thread-safety comment).

## Next

1. Lead: confirm F1-F3 against the evidence below; land after detent:consensus-transport, merging
   main first (main changed 12 files since the branch point, e.g. CMakeLists.txt).

## Blocked on

- File set (lead): the mechanism needs `include/consensus/transport/links.h` (routes, removal,
  the epoch check) and a minimal `host_transport.h` change (`admit`, `remove`, `call`'s route
  check, `nodes_` gone: +18/-4 lines; members `admit_node`/`remove_node`). Neither is in the quest's files; the lead's brief allowed
  host_transport.h minimally. Recommendation: add both to the file set. Detent's
  consensus-transport touches host_transport.h in other hunks (constructor checks, `store`); the
  merge is textual only near the constructor.

## Evidence

All at consensus:803442f unless noted. CI 44858867076 green at 803442f on every platform
(macos-arm64/x86_64, linux gcc/clang/arm64, asan-ubsan, tsan, freebsd); linux-tsan ran all 11
admission_nodes cases, 103/103 passed. The three exit-8 jobs at 803442f are the mutants failing
their cases (epoch_ca/epoch_pins: "removed before its hellos, it becomes no link"; issue: "an
impostor replaying the CA certificate is refused"; save: "the key's bytes on disk are its owner's
alone"), as intended.

| Criterion | Evidence |
|---|---|
| A1 | refused_pins 261121-112337-27fd23, refused_ca 261121-095253-b8cc25, remove_ca 261121-095252-330d78, issue 261121-113224-3a60bf, every case 261121-131011-10a70a; fails without: no-pin-check 261120-173338-bff111, no-repin-check 261120-173341-5fae97, 261120-105031-bebe74 |
| A2 | remove_ca 261121-095252-330d78, epoch 261121-113224-8358a1; TSan: CI 44858867076 linux-tsan (remove_*, epoch_* pass); fails without: remove-keeps-links 261120-173345-1b7fb0, links-no-epoch 261120-185145-6600d3 and at 803442f 261120-190024-386b01, 261120-105033-4b9fdb |
| A3 | every admission case under both policies (`_ca`, `_pins`) 261121-131011-10a70a |
| A4 | `docs/mechanisms/admission.md` (consensus:674bb21) |
| A5 | issue 261121-113224-3a60bf, save 261121-113225-813081; fails without: any-token 261120-173348-7a0880, join-unchained-issuer 261120-185148-3c31ce and at 803442f 261120-190026-ce7445, save-chmod-after 261120-185150-a20006 and at 803442f 261120-190027-3570be, 261120-105035-bb5634 |

Messages: the coordinator's D149 (default policy is the cluster CA): done in consensus:4bf833f —
`NodePins(local, pins, context)` builds `ClusterCa(context, local)`; `pre_approved_pins()` is the
alternative; the `ca` test cases run the default.
D150 (A5, issue on join): done in consensus:7d767c6 — `admission_join.h`: join token
(`DTJT`, CA fingerprint + 32-byte secret), `join()` (own P-256 key, CA pinned by the token before
the secret is sent), `Issuer` (constant-time secret check, `assign`/`issued` hooks, signs a
certificate under the CA policy's prefix), `JoinFault`; `issue` test case; mutant `any-token`.

## Result

consensus:803442f on quest/xapiand/node-admission (pushed; CI 44858867076 green). `NodePins` is a shared,
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
