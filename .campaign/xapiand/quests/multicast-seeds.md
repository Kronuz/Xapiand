# multicast-seeds: nodes on one network find each other with no flags

## Outcome

consensus gains a seed source beside its peer DNS lookups (`docs/mechanisms/peer-dns.md`): a node
announces itself by UDP multicast and learns other nodes' addresses as seeds. Joining and membership
stay the transport's TLS hello with node certificates, so an announcement is only a hint. Xapiand's
cluster (D141) and Detent use the same source for the zero-flag demo; `--join` seeds stay the way on
Kubernetes and clouds. One mechanism in the shared library, never one per product.

## Criteria

- A1 Three processes on one host find each other through the multicast source alone and report the
  same seed set; with multicast unavailable (no route, sandbox) the source reports that once and the
  node continues on its explicit seeds: tests
- A2 An announcement is bounded and versioned, carries a cluster id and the sender's services, each
  a name and its listening port (one host may advertise several: Detent's voters and relays, Xapiand's
  node), and a listener asks for the services it seeds; and a malformed, oversized, wrong-version or other-cluster datagram is dropped with a
  counter, never a seed; the decoder is fuzzed (a fuzz target and a replay corpus in CI): tests
- A3 A forged announcement for an address that then fails the TLS hello adds no member and is
  backed off, not retried in a loop: tests
- A4 Announcement rate is bounded (one per interval with jitter, a burst on start) and the group,
  port, interface and TTL are options with the Xapian-era group and port as defaults: tests
- A5 [artifact] `docs/mechanisms/seeds.md` in consensus: the datagram, its limits, what it trusts

## Tasks

T1 the datagram codec and its fuzz target; T2 the announce and listen loop on the host's loop with
its options; T3 tests A1-A4; T4 the mechanism note. Xapiand's wiring (`--discovery-*` options) is a
task of `cluster`; Detent's wiring is Detent's.

## Decisions

- D141 (owner): multicast as a seed source only. The owner, 2026-11-20: the ~300 lines live in a
  shared library so Detent gets the same demo experience; consensus already owns peer DNS lookups, so
  it is the home (no new repository).

## When done

One review, lens `security` (an unauthenticated UDP input), carrying correctness.
