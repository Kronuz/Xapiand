# node-admission: consensus admits a node's address and pin into a running transport

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status node-admission`.

## Outcome

A running consensus transport admits a new node: its address and TLS pin are added (and removed)
after construction, so Xapiand's `--join` and multicast seeds (consensus:seed-discovery) can grow a
cluster. Today `peers` and `node_pins` are fixed when the transport is built (cluster journal B2).
Which credential makes a joining pin trusted is a policy the caller passes; the default is the
cluster CA as trust anchor, pin recorded at first join (D149). Reviewed by Detent's lead; lands after detent:consensus-transport.

## Acceptance criteria

- A1 A node added at runtime is dialed and completes the hello against its admitted pin; a node
  never admitted, or presenting a different pin, is refused with a named failure (test plus a
  `--fails-without` mutant that skips the pin check).
- A2 Removing a node closes its links and refuses its later dials; adding and removing while
  traffic flows on other pairs loses no in-flight request on them (TSan job).
- A3 The trust policy is the caller's: a policy that accepts a CA-signed certificate and records
  its pin at first join, and one that accepts only pre-approved pins, both pass the same tests.
- A4 [artifact] `docs/mechanisms/admission.md`: how admission, removal and the trust policy work.
- A5 The CA policy issues a joining node's certificate in the handshake in exchange for the join token
  (D150): a wrong or missing token is refused with a named failure, and a node never needs a key or
  certificate configured by hand (test plus a `--fails-without` mutant that accepts any token).

## May land early

Nothing: the admission path is the part the review exists for.
