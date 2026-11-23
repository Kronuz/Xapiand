# consensus-join-v2 review: security

Commit: consensus:45f2309320716df816280887cc040840aa7a163e
Verdict: CHANGES
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Threat model (owner): trusted cluster. The attacker is an unauthenticated peer on the transport
port, a holder of a stale or wrong-cluster token, or a replayer; never a compromised admitted node.

## Evidence I relied on

- A2 mutants sign-each-time 261123-132820-5a61c4 and save-in-place 261123-132818-a116ef, A4
  pin-der 261123-133811-fee953: killed, as the lead stated. Not re-run.
- The contributor's queued jobs (A3 join-no-cap/slot-deadline/rate, A4 reload-keeps-links,
  reload-keeps-trust, redial-at-deadline, A5 skip-boot-check, A6 fuzzers) and CI run 45285292077
  have no result yet; I judged A3-A6 by reading the code and the tests, not by those results.

## What I checked myself

Probe scripts in `consensus-wt/xapiand/consensus-join-v2/.scratch/xapiand/review-consensus-join-v2-security/`:

- `csr_cost.cc`, `csr_cost.sh`: calls the real `join_detail::signed_key` (what `JoinService::serve`
  runs on every request's CSR, `admission_join.h:530`, and `Issuer::issue` again, `:649`) on a
  valid P-256 CSR and on forged CSRs that need no private key: an RSA key of OpenSSL's largest
  modulus (16384 bits, 64-bit exponent) with a random signature, and a DSA key with p of 10000 bits.
  Run under `quiet light` (host load 40 on 16 CPUs, so absolute times are loose; the ratio is the
  point), output `out/csr_cost.txt`:
  P-256 0.259 ms, forged RSA-16384 10.078 ms (4192-byte CSR, verification fails, cost paid),
  forged DSA 0.130 ms. Recorded as job 261123-152257-24951c (pending at writing).

By reading, against each question:

1. Token v2 (Q1). `decode_join_token` (`admission_join.h:83-106`) fixes the length, magic and hex
   before decoding; v1 shape and other magics are `version`. The joiner checks the offer's cluster
   against the token's, picks the offered CA whose `key_pin` equals the token's pin, and requires
   the member's TLS leaf to chain to it (`:786-797`) before the secret is written (`:798`). A
   forged CA certificate carrying the real CA key gains nothing: the leaf must still verify under
   that key, and TLS proves the leaf's key. The member refuses another cluster's request (`:534`).
   The secret is compared as SHA-256 digests with `CRYPTO_memcmp` (`:115-118`). Expiry, revocation
   and use counts are the hook's (Xapiand M4) by design; `Issuer::issue` calls `assign` on every
   request, retry window included (`:653`), so a revoked token is refused inside the window too.
   A token for cluster A cannot admit to B: B's member refuses A's cluster id, and B's hook holds
   only B's digests. No finding.
2. Join budget (Q2). A joiner sits in the handshakes' pool only until its first byte
   (`links.h:248-268`) and then in the join pool (`pool`, `per_source`, token-bucket `rate`, `slot`
   and `answer` deadlines, `admission_join.h:486-608`). Allocations before authentication are
   bounded: frames at 64 KiB checked before allocation (`:355-362`), offer CAs at 16, secret and
   reason at 255. The work is not: see S1 (CSR verification) and S2 (source map scan).
3. Decoders (Q3). Token, offer, request, answer and DTHL0003 decoders check every length before
   taking it; the hello is fixed at 180 bytes with `reason` only alongside `refused`. The three
   fuzz targets cover every decoder with round-trip oracles and run the DER through `d2i_X509`,
   `d2i_X509_REQ`, `X509_REQ_verify` and `key_pin`. Not fuzzed: `read_frame`, the magic and
   `first_byte` peek, and `serve`'s sequencing; each is short and bounded on reading.
4. Reload and pins (Q4). `NodePins::reload` swaps the trust store under the directory's lock;
   removals and pins are untouched, so a removed node's key stays refused (`owner`,
   `node_pins.h:208-215`) after any reload, renewal included. `authenticate` and `current()`
   re-verify the peer's chain against the current store, and `Links::reload` aborts links and
   offers that fail it on each loop; anything reserved between the swap and the posted
   `Links::reload` passes `current()` against the new store. The verify store matches the
   handshake's (reactor sets no CRL or partial-chain flags), so no node loses trust a handshake
   would grant. A join-path TLS session resumed on the node path carries no leaf and is refused
   ("no peer certificate"). No finding.
5. Redial fix (Q5). `pump` (`host_transport.h:1013-1021`) only arms a timer at `dial_after`'s end,
   once (`!redial`); dialing stays behind `now < dial_after`, and the existing 10 ms re-arm only
   fires while the spacing has not passed. A removed peer has its calls failed by `remove_node`,
   so nothing arms. No storm, no bypass. No finding.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| S1 | yes | defect | An unauthenticated joiner makes loop 0 (where every accept and every join runs) verify a CSR of any key type and size. A forged RSA-16384 CSR costs 10.1 ms per request against 0.26 ms for the P-256 key `fresh_join_key` makes, about 39 times, and needs no private key, so the attacker precomputes it once. Within the per-address rate (10 a second) that is about 0.1 CPU-second a second per address: ten IPv4 addresses, or one IPv6 /64, keep loop 0 saturated, with up to 64 verifications (about 640 ms) queued ahead of each established node's handshake step and each loop-0 link's frames. The doc's budget ("at most 10 TLS handshakes a second per address on loop 0", admission.md, The join pool) understates per-joiner work by that factor, and the `flood` case uses P-256 joiners from one address, so A3 is unproven for this flood. In Xapiand every such request also reaches the root's leader, whose `Issuer::issue` verifies the CSR again before `assign` checks the secret, so all members' pools converge on one node's CPU. | csr_cost probe (`out/csr_cost.txt`, job 261123-152257-24951c); `admission_join.h:530`, `:649-653`; `flood_case`, `admission_nodes_test.cc:1062` | Bound the verification work before it runs; the owner chooses: (a) accept CSR keys from an allow-list (EC P-256/P-384, Ed25519, RSA at most 4096 bits) and refuse the rest `malformed` before `X509_REQ_verify`; (b) keep any key but verify off loop 0 under its own concurrency cap; (c) keep it and document the cost per key size in the budget table. Under (a) or (b), `Issuer::issue` also checks the key type before verifying. Test: a request with a forged RSA-16384 CSR is refused `malformed` (a), or loop 0 answers a node's echo within its usual time while 64 forged requests run (b); the probe reruns with the forged cost at or below P-256's. |
| S2 | no | defect | `JoinService::take` scans the whole source map under the service's mutex on every call once it holds more than 256 addresses, and only erases entries idle past a second. A flood from many addresses (an IPv6 /64) keeps about one entry per connection per second, so each accept costs work proportional to the connection rate: quadratic in it overall, on loop 0, refused connections included (the scan precedes the pool check). | `admission_join.h:593-595` | Prune at most once a second (or from a time-ordered list), and cap the map, treating a full map as `busy`. |
| S3 | no | gap | Eight addresses (or one IPv6 /64, as the doc says) hold all 64 join slots with stalled joiners, each for the slot deadline, and newcomers are refused rather than evicting: joining is unavailable for as long as the flood lasts. A3 covers established nodes, not joiners, so this is a note. | `admission_join.h:590-607`; admission.md, The join pool | Owner's call: keep it (joiners retry), or prefer evicting the oldest slot still before its request, as the handshakes' pool does. |
| S4 | no | nit | A node removed from the directory whose certificate is still valid can pose as a member to a joiner (the joiner checks only the chain to the pinned CA) and take its secret. This is the design review's F9 under another holder; the trusted-cluster model and the joiner reaching the operator's endpoint keep it a note. | `admission_join.h:795-797`; cluster-membership-design F9 | Say in admission.md that a removed node's unexpired certificate can receive a joiner's secret; short certificate lifetimes bound it. |
| S5 | no | nit | Every well-formed request of the member's cluster reaches the hook with any secret; in Xapiand that is a relay to the root's leader per unauthenticated request. | `admission_join.h:547-549` | Xapiand's hook could rate the relay; no consensus change needed. |

## Pending probes

- 261123-152257-24951c: `csr_cost.sh` at consensus:45f2309 (the same probe I ran under `quiet
  light`; its log confirms S1's numbers on a quieter host).

## Not run

- No end-to-end run of a forged-CSR flood against a node's hello deadline; S1 rests on the
  measured per-request cost and the pool arithmetic.
- No S2 measurement; it is by reading.
- The contributor's queued mutant, fuzz and CI jobs; I did not rerun or wait for them.
- No TSan or ASan build of my own; Linux and other platforms are CI's.
- Xapiand's hook (token life, uses, revocation) is out of this quest.
