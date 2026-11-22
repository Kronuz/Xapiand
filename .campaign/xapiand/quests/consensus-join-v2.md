# consensus-join-v2: consensus joins a node by token v2 on the transport port, pins keys, reloads certificates and checks the boot

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status consensus-join-v2`.

## Outcome

Consensus carries the membership protocol Xapiand's cluster needs for `join` (cluster A4): additions
1-5 and 7 of the reviewed design's "What consensus must add" (Xapiand
`docs/design/cluster-membership.md` on quest/xapiand/cluster; review
`.campaign/xapiand/reviews/cluster-membership-design.md`). A join token names its cluster and pins
the CA's key. The join exchange runs on the transport's port, with its own budget, so joiners can't
starve established nodes. Node pins follow the key rather than the certificate, so renewing a
certificate keeps its pin. Certificates and bundles reload at runtime. Every acceptor admits a hello
only for the boot the directory admitted (M7), and refuses with a reason.

## Acceptance criteria

- A1 Token version 2 carries the cluster id and the CA's SPKI pin. A version 1 token, a token for
  another cluster id, and a token whose CA key doesn't match the pin are each refused with a
  distinct reason: the `suite` check (admission tests)
- A2 The issuer hook is asynchronous and idempotent per key. Two joins with the same saved key get
  one certificate. `join` uses the key its caller saved. `Joined::save` replaces the file by rename,
  so a crash before the rename keeps the old file whole: the `suite` check, plus a mutant that
  writes in place, which a crash test must catch
- A3 The join exchange runs on the transport's port, with its own pool, rate and slot deadline (M4)
  outside `handshakes`. Joiners flooding past the rate don't push an established node's hello past
  its deadline, and a joiner that stalls is cut at the slot deadline: the `suite` and `tsan` checks
- A4 Node pins are taken over the key's SPKI. A renewed certificate for a pinned key is admitted
  with no operator step, and a new key under a pinned name is refused. Reloading a certificate or
  bundle at runtime closes every link whose chain no longer verifies. The hello re-checks the chain
  and expiry, so an expired certificate on the shared port is refused: the `suite` check
- A5 The hello carries the boot and the directory version that admitted it. Every acceptor checks
  them against the directory's admitted boot, and the refusal names the reason. A mutant that skips
  the boot check must fail a named test, on a tree that compiles: the `suite` check, plus that
  mutant's job, whose log shows the test failing
- A6 The token v2, join exchange and hello decoders reject malformed input without crashing or
  leaking: one fuzz job per decoder, at least 30 CPU-minutes each, under ASan and UBSan
- A7 [artifact] `docs/mechanisms/admission.md` and `docs/mechanisms/hello.md` describe token v2,
  the join exchange and its budget, key pins, renewal and reload, and the boot check, with each
  limit and what it costs
- A8 The `asan` and `tsan` checks pass, and CI is green on every platform: [ci]

## Contracts

- Provides `consensus:join-protocol`: the token v2 encoding (cluster id, CA SPKI pin, secret), the
  join exchange on the transport port, the issuer hook's signature, and the hello's boot and
  directory-version fields with their refusal reasons. Bytes may change until the first release
  (formats open until release). Each change is in the mechanism notes, with a pre-release version
  bump.
- Consumes `consensus:node-admission` (`HostTransport::admit` and `forget_node`, the trust policy).
- Detent consumes consensus' transport (its `Seeds` wiring). Its lead hears of the start and reviews
  any change to the API it calls.

## Decisions

- Lead, 2026-11-21: this is a quest of its own, not a widening of node-admission. node-admission has
  a correctness review in hand and covers runtime admission alone. This quest needs node-admission,
  since it builds on the same files.
- Lead, 2026-11-21: addition 6 (a forced configuration under a new epoch, and replica state for
  salvage, F23) is left out. It belongs with cluster's salvage work, not with joining.
- The trust model is D149 (the cluster CA is the trust anchor, pins recorded at first join), D150
  (one join path, always TLS) and D152 (membership by commands).
- Lens: security. One reviewer, who also carries the protocol's correctness questions: the rate
  budget, idempotency, and the boot check under reordering.

## May land early

None: a wire protocol and a security surface. Everything waits for the review.

## Tasks

None: one contributor (implementation, strong, high).
