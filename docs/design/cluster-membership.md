# Cluster membership

How nodes get identities, join, leave, and survive lost disks and nodes, copied directories,
expiring certificates and a lost quorum, with no key ever handled by an operator.

- Status: revision 1, 2026-11-20, for re-review (lens `design`); nothing is built. It folds in the
  design review (`.campaign/xapiand/reviews/cluster-membership-design.md`) and D153, and closes
  D150 to D152. Read at Xapiand `6e7ae7926`, consensus `e1f249d` and `64dbb06`, and pki `70a3b76`,
  under the trusted-cluster threat model.
- Models, extended from the review's, in the quest worktree's `.scratch/cluster-design/`:
  `m7_admission.py` (job `261120-122335-4b19c5`) and `m9_survivor.py` (job `261120-122337-e67e91`).

## Abstract

Plain `xapiand` runs a node; on an empty directory it founds a cluster of one, with no cluster port
until `xapiand invite` mints a join token. The membership commands do one operation and exit (D153).
A dead node's replicas move automatically; identities go only by `leave` or `remove`, data only by
`recover --accept-loss`. Trust lives in the root group's node directory, not in the certificate
authority (CA), whose key only the root's replicas hold.

## Commands and directory states

| | This node | Another node |
|---|---|---|
| In | `xapiand join TOKEN` | `xapiand invite` |
| Out | `xapiand leave` | `xapiand remove NODE` |

Only plain `xapiand` runs a node; `leave` streams its drain's progress until the node is retired.
`xapiand recover` forces a group past a lost majority (M9). All but `join` call the running node's
HTTP API. A deployment that must never found runs `xapiand join TOKEN` before every start (systemd
`ExecStartPre`, a Kubernetes init container).

| The data directory holds | `xapiand` | `xapiand join TOKEN` |
|---|---|---|
| nothing | founds a cluster of one | joins, never founds |
| a cluster of one with no live token | runs | joins, carrying its indexes (M3) |
| a member of the token's cluster | runs | exits 0 unchanged, without the network |
| a member of another cluster, or a cluster of one with a live token | runs | refused (`cluster`), naming both ids |
| an identity the cluster refuses | exits naming the reason and what to do (M7) | the same |

## Decisions

**M1. Identities.** A cluster is a 16-byte random id (the cluster field of consensus's hello,
`host-connection.md:99`), a name and a P-256 CA. A node is an id from the root's counter, never
reused; a P-256 key that never leaves it; a certificate for
`spiffe://<cluster id>/xapiand/node/<id>`; and a name unique among unretired nodes, by default the
host name. The root's records, the contract for `xapiand:node-directory`:

| Record | Fields |
|---|---|
| Cluster | id, name, CA certificates and key, next node id, next admission number, directory version |
| Node | id, name, key pin, certificate expiry, addresses, `active`, `leaving` or `retired`, admitted boot, admission number, pending offer (M7) |
| Token | id, the secret's SHA-256, expiry, uses left |

Two first starts found two clusters, so nothing races. An explicit `--name` held by an unretired
node is refused (`name`, TS-R2); a default one takes a suffix (`xapiand-3-2`). pki's Xapiand
identity must name the id, not the name (`pki/include/pki/identity.h:30-33`).
Criteria: cluster A8, with first starts in place of `init`.

**M2. A cluster of one, and one-shot commands.** `xapiand invite [--ttl D|never] [--uses N]` mints a
token (M4) and prints a `join` line seeded at this node, first opening a cluster of one's cluster
port (default 9880, `5a517b052^:config.h.in:285`) and announcements. `join` reads the directory
first, so on a member it never waits on the cluster; otherwise it finds the cluster by multicast or
`--seed HOST[:PORT]`, retries until it answers (cluster A9), saves the identity by rename and exits.
It reads `XAPIAND_TOKEN`, since other users can read arguments. D152's `init` goes.

A lost disk under plain `xapiand` founds a new cluster of one, which splits no membership (TS-R6):
it neither announces nor holds a token, and old peers refuse its certificate (Limit 1, Q7).
Refusing an empty directory (D152) would fail every newcomer, so founding warns and `GET /` shows
the node count.
Criteria: plain `xapiand` binds only the HTTP port and sends no datagram for 10 s; after `invite`,
`join TOKEN` then `xapiand` serves from a second directory; `join TOKEN` again exits 0 with every
other node stopped, the directory's bytes unchanged.

**M3. Carrying a cluster of one's indexes in.** The join lists the node's index paths
(`.xapiand/*` excluded) for the root to reserve; a path in use refuses the whole join, naming the
paths (nested paths are distinct keys). After the commit the node drops its old CA, tokens and key,
closes each group (a close checkpoints the applied position, `cluster-release.md:303`) and
re-founds it from that checkpoint as a lone-voter group of the new cluster, copying no bytes
(strata must add this), with a marker per shard so a crash resumes; placement then grows each index
to its `number_of_replicas` (D7). At 1,000 creations a second
(`cluster-release.md:221`), 10,000 indexes take 10 s of root commits.
Criteria: a cluster of one holding `a` and `b/c` joins 3 nodes, both reading equal everywhere; a
collision on `a` is refused naming it, both sides unchanged; one with a live token is refused; a
kill at each carry step restarts to one whole state (a crash-point job).

**M4. Tokens and the join exchange.** A token is one string, consensus's codec version 2: the
cluster id, which picks its cluster's announcements (`seeds.md:18`); the SHA-256 of the CA's public
key (SPKI, as kubeadm pins); and a 32-byte secret, printed once and never logged, whose hash alone
the root keeps. `POST`, `GET` and `DELETE /:cluster/tokens[/<id>]` mint, list and revoke; rotation
is mint, then revoke.

Any member serves joins on its cluster port to TLS clients without a certificate, in their own
pool: at most 64 at once and 8 per source, refused past the cap rather than evicting links, and a
source opening more than 10 a second delayed. The joiner saves its key before its first request and
sends the secret only to a member whose chain matches the token's pin; the member relays to the root
leader, where one command checks token and name, allocates the id, records the pin and counts the
use, and the leader signs with the CA key current at that commit, setting its own extensions. A
retry with the saved key, after a lost answer or a crash, gets the same node and burns no use.
Criteria: each refusal (`token`, `expired`, `revoked`, `used`, `name`, `cluster`) beside an accepted
join; a one-use token at two members admits one node; a root leader or a joiner killed after the
commit retries as the first id, the use kept; 64 concurrent joins at one member while links redial
drop no link handshake; no log holds a secret.

**M5. Pins live in the root's directory.** Each node applies the directory to its transport with
consensus's `admit_node(node, address, pin)` and `remove_node(node)` (`host_transport.h:341-348`,
node-admission branch), besides verifying chains against the CA bundle; the keepalive answer
carries the directory's version (`src/cluster/rpc_methods.h:140-146`), and a node behind fetches the
changes. This refines D149: the pin is recorded at first join, in the root, so no peer trusts on
first use. Pins hash the key (addition 4), so renewals keep them.
Criteria: a retired node is refused, its links closed, everywhere within 2 s; a mutant trusting
first use fails.

**M6. Leaving, dead nodes and removal.** Replicas move automatically; identities never do.

| Event | The node's replicas | Its identity |
|---|---|---|
| `xapiand leave` on it | drained by D7's steps after its leaderships and any root replica, with the CA key, are handed off | retired, its directory deleted (Q6) |
| `down` past `replace_after` | replaced | kept: it rejoins as itself |
| `xapiand remove NODE` | lost with it; a group left without a quorum of unretired voters waits for `recover` (M9) | retired; an `up` node needs `--force` |

`leave` refuses when the remaining nodes cannot hold the root or an index's `number_of_replicas`,
naming the indexes (Q5); check and mark are one root command counting only nodes neither leaving,
retired nor down, so concurrent `leave`s and `remove`s see each other. The drain is the root's
record: a killed `leave` leaves it running (`leave --cancel` undoes it), and a node dying while
leaving retires once its replicas are replaced. `replace_after` is one cluster setting (default
300 s). Retiring revokes (TS-R5): every node drops the pin, so neither the node nor a copy of its
disk connects or renews again.
Criteria: a leave on 5 nodes under writes keeps digests equal and resumes when killed at each step;
of two concurrent leaves on 4 nodes at `rf = 3`, one is refused, naming the index; a removed node's
directory copy exits `retired`; a node down past `replace_after` rejoins as itself.

**M7. Copies, restores and backups.** Each process start is a boot, a random id
(`rpc_methods.h:134`). The hello carries it (consensus addition 7), and every acceptor, root
replicas included, refuses any boot but the node's admitted one with `duplicate` while that one was
heard within 2 s; a new root leader counts the 2 s from its election, and a clean stop clears the
boot. An acceptor behind the directory version that admitted a boot fetches before answering.

The root leader admits a boot in three steps. The boot presents its saved admission number; equal
to the record, the root commits a fresh offer from the cluster's counter. The node saves the offer
and presents it; the root commits it as the record with the boot, and only then does the node open
groups. A boot presenting the pending offer completes it (its predecessor crashed after the save);
any other number is `stale`. A running boot refreshes the same way hourly, 2 root commits per node,
0.6 a second per 1,000 nodes.

A directory moved while stopped runs. Root replicas open the root group only once admitted; with
no root leader (the whole cluster cold), root voters apply the same checks against their own copies
of the record, and the first leader commits its voters' boots before serving. This changes D5's "a
node restarts without the root".

A refused node exits naming what to do: `duplicate`, another process runs this identity; `stale`
(an older copy) or `unknown` (a root recovery lost the admission, M9), empty the directory and
`join`; `retired`, the start finishes the deletion. A node whose handshakes fail its pin asks the
join exchange by its key, which answers `retired` or `unknown`.

In the model, the review's two-step rule admits an original after its copy ran and died, which
the third step refuses, and a hot copy reads only `duplicate` across 2,000 trials of 20 root leader
changes.
Criteria: a hot copy beside a running original exits `duplicate` within 2 s and opens no group, the
root's included, across 20 root leader changes; a backup older than one refresh exits `stale` on
each of 100 restarts; a kill between steps restarts admitted; a cold copy that ran makes its
original `stale`.

**M8. Certificates and the CA.** Node certificates last a year and renew at two thirds of their
life, with jitter, through the join exchange authenticated by pinned keys both ways, so pins never
change and an expired certificate still renews. The 10-year CA certificate is reissued with its key
at two thirds of its life; both expiries warn at five sixths (pki's rule). The CA key is a field of
the root's cluster record, held by the root's replicas only (D151's "every voter" read as the
root's).

`POST /:cluster/ca/:replace` makes a new key, or takes an operator's CA as one PEM file (D150),
waits for joins in flight, renews every active node, drops the old CA, closes links whose chain no
longer verifies, and revokes every token: one procedure for a compromise, a loss and an operator's
CA. Rejected: pki's offline CA (D113 Q5 (b)), which hands an operator the key, and escrow (TS-R8).
Criteria: at a 60 s lifetime every node renews under load, no link lost; a node stopped past expiry
renews; after a replacement, old-CA certificates and tokens are refused; a file scan, also after a
root replica moves away, finds the key on root replicas only, and no route or log carries it.

**M9. Forced recovery.** `xapiand recover [PATTERN] --accept-loss` is the only way past a lost
majority, never automatic: for each group of the indexes PATTERN matches (A3) whose unretired voters
form no quorum, after `remove` (M6), or for the root (no PATTERN) while most root voters are
unreachable. Among the survivors holding every entry up to the highest commit index any survivor
heard, it keeps the most up to date as a vote compares (last term, then last index). In the model,
the longest log loses an entry another survivor heard committed in 239 of 40,000 five-voter survivor
pairs, the vote's comparison in 3, this rule in none, nor in 35,000 seven-voter triples. Without
`--accept-loss` it prints the plan per group: the survivor kept, the entries other survivors hold
that it lacks, and that entries only the lost voters held are lost unlisted (1,827 of those pairs).
With it, each membership is forced to its survivor under a new epoch (consensus addition 6), or
re-founded empty, and placement regrows it; writes answered `unknown_outcome` may appear, and
returning former voters are refused by the epoch (`host-transport.md:34`).

The root's recovery loses the commits its survivor lacks: nodes admitted then are answered
`unknown` (M7), index records are rewritten from the groups' metadata (strata must record each
group's index path), and the node id counter skips past the highest id any reachable node or group
membership names. With every root replica lost, a node's directory copy seeds it and the CA is
replaced (M8).
Criteria: the model's traces `trace_vote` and `trace_index` as tests of the forced configuration;
an `rf = 1` index whose node was removed answers 503 naming `recover` until recovered empty; with 2
of 3 root voters killed and emptied, recovery regrows the root and admits a node, refusing a former
voter's copy; `recover` refuses while most root voters answer.

**M10. Upgrades.** Master's data directory starts as a cluster of one with its indexes; a Xapian-era
multi-node cluster moves by dump and restore. `--solo` and `--cluster` go
(`5a517b052^:src/opts.cc:219-220`, `:381-382`).
Criteria: master's single-node directory serves its indexes.

**M11. Containers and Kubernetes.** A StatefulSet with volumes runs `xapiand join` in an init
container and `xapiand` in the main one, with `XAPIAND_TOKEN` from a Secret holding a `--ttl never`
token and `--seed` naming the headless service. Bootstrap: `xapiand-0` starts alone, `xapiand
invite --ttl never` there fills the Secret, and the init container is added, a no-op on `xapiand-0`
(zero-touch is Q4). Scale-down runs `leave` in the highest pods before lowering `replicas`; a
preStop hook would drain on every rolling restart.
Criteria: a StatefulSet-shaped harness (empty directories, one token, one DNS seed) scales 3 to 5
to 3 through `leave` and restarts each node empty in turn, green between, no `quorum` write lost.

## What consensus must add

1. Token version 2: the cluster id, and the CA's SPKI pin instead of its DER
   (`admission_join.h:25-60`).
2. An asynchronous issuer hook, idempotent per key (`admission_join.h:185`, `:196`); `join` taking a
   key its caller saved, where today it makes one per call (`admission_join.h:338`); `Joined::save`
   by rename.
3. The join exchange on the transport's port (today a separate socket, `admission.md`), with its own
   pool and rate (M4) outside `handshakes` (`host-connection.md:136-145`).
4. Node pins over the key's SPKI (today the leaf's DER, `tls.h:36-44`).
5. Renewal by pinned keys, and runtime certificate and bundle reload closing links whose chain no
   longer verifies (TS-R12); the hello path re-checks chain and expiry, since the port also takes
   clients without valid certificates.
6. A forced configuration under a new epoch (`ARCHITECTURE.md:31`), and each replica's last term,
   last index and heard commit index.
7. The boot in the hello and a reason in its refusal, checked by every acceptor against the
   directory's admitted boot (M7).

strata adds founding from a checkpoint (M3) and index paths in group metadata (M9).

## How others do it

| Case | etcd | CockroachDB | Elasticsearch 8 | kubeadm | This design |
|---|---|---|---|---|---|
| Joining | `member add`; operator certificates | `--join`; operator CA | token: CA fingerprint, addresses, API key; 30 min | token, 24 h, and the CA key's hash | token: cluster id, CA key pin, secret |
| Dead node | `member remove` | replicas moved after 5 min | shards moved after 1 min | `kubectl delete node` | replicas moved after `replace_after` |
| Lost quorum | `--force-new-cluster` | `debug recover` | `elasticsearch-node unsafe-bootstrap` | etcd's | `recover --accept-loss` |

## Limits

1. **Silent:** a lost disk under plain `xapiand` serves routed clients an empty cluster (M2, Q7).
2. **Silent:** a copy admitted while its original is cut off from the root past 2 s, a backup taken
   since its node's last refresh and restored after the node died, or a root replica restored while
   the root has no leader runs on duplicated or rolled-back state and can vote twice in a term (M7).
3. **Silent for clients:** a group recovered while its removed voters live behind a partition runs
   twice until it heals, losing what the far side acknowledged; `remove` takes the operator's word.
4. A root replica's backup holds the CA key: it can impersonate the cluster to a joiner and take
   its secret, so it admits no node without a live token.
5. A node restarting while the root has no quorum waits (M7); below three nodes the root has one
   voter, whose loss needs `recover`.

## Other review findings

- F7: the key current at the join's commit signs; a CA replacement waits for joins (M4, M8).
- F10: added to Q3.
- F11: one SPKI pin kind for Xapiand and Detent, to agree with Detent's lead.
- F13: the carry closes each group first; any cluster of one without a live token carries.
- F14: M8's key scan runs after a root replica moves, and covers routes and logs.
- F15: M7 names the refresh's cost (Limit 2 its window).
- F16: `recover` takes an index pattern.

## Questions for the owner

- **Q1. Token defaults?** Default: 24 hours (kubeadm's), unlimited uses within them, `--ttl never`
  allowed.
- **Q2. Carry a cluster of one's indexes in (M3), or join only when empty?** Default: carry.
- **Q3. Token routes on the unauthenticated API?** Anyone reaching it can already delete every
  index, and a node it admits may become a root voter holding the CA key; the routes take the API
  token once it exists (TS-R9). Default: yes.
- **Q4. Zero-touch bootstrap of identical pods** by a pre-shared secret? Default: not in the
  release.
- **Q5. Downsizing below `number_of_replicas`: refuse, or warn and lower it?** Lowering changes
  users' durability. Default: refuse, naming the indexes.
- **Q6. `leave` deleting the drained node's data?** Its root replica, with the CA key, goes either
  way. Default: yes.
- **Q7. The founder's deployment (F12)?** A founder kept on plain `xapiand` founds again after a
  lost disk (Limit 1). Options: (a) `invite` also prints the founder's own `join TOKEN` line for
  its deployment; (b) plain `xapiand` refuses to found while `XAPIAND_TOKEN` is set; (c) keep
  Limit 1. Default: (a).

## Status and first steps

Steps: consensus additions 1 to 5 and 7 (node-admission holds version 1); `membership` (M1, M2, M4
to M7); renewal and CA replacement (M8); `recover` and addition 6 (M9); the carry (M3). The lead's
criteria changes:

1. A4 drops loopback plaintext (D150) and `--join`.
2. A7: plain `xapiand` founds on an empty directory, `invite` mints, `join` saves and exits (D153);
   `init` and the empty-directory refusal go.
3. A8 founds at first start, not by `init`.
4. A9 adds that `join` on a member exits 0 without the network.
5. A10's token must be live (Q1), the new node admitted through any member.
6. New: M7's criteria, M9's traces, M4's concurrent joins and joiner crash, M6's concurrent leaves.

Review first: M7's admission steps and the root's cold start, M9's survivor rule, M4's
unauthenticated pool, Limit 2.
