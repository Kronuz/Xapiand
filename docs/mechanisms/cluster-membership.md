# Cluster membership

How a node's process start is admitted and how the root leader tracks which nodes are alive. The
design is `docs/design/cluster-membership.md` (M7) and `docs/design/cluster-release.md` (D6); this
note describes what `src/cluster/membership_admission.*` and `src/cluster/membership_liveness.*`
do. Joins, tokens, pins and the transport wiring are not built yet.

## Admission: three steps per boot

Each process start is a boot with a random id. Its directory holds the admission number it last
saved. The root's node record holds the confirmed number (`record`), a pending offer and the boot
the offer was made to, and the admitted boot. `admit(node, next, boot, number)` is one root
command:

| Presented number | Answer | Record change |
|---|---|---|
| below `record` | `stale` | none |
| the pending offer, by the boot it was made to | `admitted` | `record` = number, the boot admitted, the offer cleared |
| anything else | `offer` | a fresh offer above the record, the number and every number issued, to this boot |

The node saves an offer before it presents it, and opens groups only once admitted. Only the boot
an offer was made to completes it, so a copy of a directory that saved an offer gets its own offer,
and whichever confirms first makes the other's number fall below the record. Every directory older
than the last confirm is therefore `stale`. A running boot refreshes hourly with the same two
steps.

A number above the record means the record lags the node, as after a root salvage that lost a
refresh or the join (review F17); it gets an offer like any other number and the node is
re-admitted with its data. The largest `uint64` has no offer above it and is `stale`; the root's
counter never issues it.

While the root has no leader, a root voter answers another voter against its own copy of that
voter's record (`cold_check`): `stale` below it, `accepted` at the record or the pending offer, and
`wait` above it, since the copy may lag and only a leader decides.

The unit test `histories` runs the model's random histories (`m7_admission.py` `explore()`, without
salvages): 4,000 histories of 40 steps of starts, refreshes and kills at each step, and copies. It
checks that no directory is admitted after another confirmed since the two diverged, and that the
last directory to confirm is never locked out.

## Liveness: the keepalive table

Every node sends a `keepalive` to the root leader every 500 ms. The leader's `LivenessTable` keeps,
per node, the boot it last heard and when. A sweep judges each node by its silence: `suspect` after
two intervals (1 s), `down` after three (1.5 s), and returns only the transitions, which are what is
logged. A keepalive from a suspect or down node returns its transition back to `up`; a new boot (a
restart) replaces the entry. A new leader builds its table at its election and counts every
directory node's silence from then, so an election marks nobody down before 1.5 s. With sweeps
every 250 ms, a killed node is logged `down` within 2 s of its last keepalive (A4).

The same table answers M7's `duplicate`: another boot of a node is refused while the node's
admitted boot was heard within 2 s. A boot the table never heard counts as heard at the election,
so a new leader refuses copies for 2 s before it can know the original is gone. The admitted boot
comes from the directory, the last-heard time from the table; a clean stop clears the admitted
boot, so the next start is never `duplicate`.

The table is linear in nodes at the leader (cluster-release section 10) and owned by one loop.
