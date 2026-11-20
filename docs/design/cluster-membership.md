# Cluster membership

How nodes get identities, join, leave, and survive lost disks and nodes, copied directories,
expiring certificates and a lost quorum, with no key ever handled by an operator.

- Status: design, revision 0, 2026-11-20, for review (lens `design`); nothing is built or measured.
  It closes drafts D150 to D152 (`.campaign/xapiand/PLAN.md`) under the owner's direction that
  plain `xapiand` runs as one node.
- Read at Xapiand `6e7ae7926`, consensus `e1f249d`, `64dbb06` (node-admission) and `8213720`
  (multicast-seeds), and pki `70a3b76`, under the trusted-cluster threat model (nodes are trusted,
  the perimeter is defended).

## Abstract

Plain `xapiand` on an empty directory founds a cluster of one, with no cluster port until
`xapiand invite` mints a join token. `xapiand join TOKEN`, every other node's run command, keeps
running: it joins from an empty directory, carries in the indexes of a cluster of one that no node
ever joined, and otherwise runs. A dead node's replicas move automatically; identities go only by
`leave` or `remove`, data only by `recover --accept-loss`. Trust lives in the root group's node
directory (cluster-release D5's metadata group), not in the certificate authority (CA), whose key
only the root's 3 or 5 replicas hold.

## Commands and directory states

| | This node | Another node |
|---|---|---|
| In | `xapiand join TOKEN` | `xapiand invite` |
| Out | `xapiand leave` | `xapiand remove NODE` |

Plain `xapiand` runs a node, and `xapiand recover` forces a group past a lost majority (M9);
`invite`, `leave`, `remove` and `recover` call the running node's HTTP API.

| The data directory holds | `xapiand` | `xapiand join TOKEN` |
|---|---|---|
| nothing | founds a cluster of one | joins; never founds |
| a cluster of one that never had a second node and has no live token | runs | joins, carrying its indexes (M3) |
| a member of the token's cluster, its founder included | runs | runs; the token goes unused |
| a member of another cluster, a cluster of one with a live token included | runs | refused (`cluster`), naming both ids |
| a retired, duplicate or stale identity | refused, naming which (M6, M7) | the same |

## Decisions

**M1. Identities.** A cluster is a 16-byte random id (the cluster field of consensus's hello,
`host-connection.md:99`), a name and a P-256 CA. A node is an id from the root's counter, never
reused; a P-256 key that never leaves it; a certificate for
`spiffe://<cluster id>/xapiand/node/<id>`, as consensus's `ClusterCa` parses; and a name unique
among unretired nodes, by default the host name. The root's records, the contract for
`xapiand:node-directory`:

| Record | Fields |
|---|---|
| Cluster | id, name, CA certificates and key, next node id, directory version |
| Node | id, name, key pin, certificate expiry, addresses, `active`, `leaving` or `retired`, admission number (M7) |
| Token | id, the secret's SHA-256, expiry, uses left |

Two first starts found two clusters, which meet only through `join`, so nothing races. An explicit
`--name` held by an unretired node is refused at admission (`name`, TS-R2); a default one takes a
suffix (`xapiand-3-2`). A name outlives its disk, so pki's Xapiand identity must name the id instead
(`pki/include/pki/identity.h:30-33`).
Criteria: cluster A8, with first starts in place of `init`.

**M2. A cluster of one, and a `join` that keeps running.** `xapiand invite [--ttl D|never]
[--uses N]` mints a token (M4) and prints a `join` line seeded at this node, first opening a
cluster of one's cluster port (default 9880, the Xapian-era remote port,
`5a517b052^:config.h.in:285`) and announcements. `join` keeps running, where D152 saved and exited,
so one deployment command covers first start, restarts and a lost disk; with an exiting `join`, the
run command is plain `xapiand`, which founds after a lost disk. `join` finds its cluster by multicast
or `--seed HOST[:PORT]` and retries until it answers (cluster A9); deployments pass `XAPIAND_TOKEN`,
since other local users can read arguments. D152's `init` goes.

A lost disk under plain `xapiand` founds a new cluster of one: it splits no membership (TS-R6),
since it neither announces nor holds a token and old peers refuse its certificate, but serves routed
clients an empty cluster (Limits 1). Refusing an empty directory (D152) would fail every newcomer,
so `join` is the documented deployment command, founding warns, and `GET /` shows the node count.
Criteria: plain `xapiand` binds only the HTTP port and sends no datagram for 10 s; after `invite`,
`join TOKEN` serves from a second directory, and again after a restart.

**M3. Carrying a cluster of one's indexes in.** The join lists the node's index paths
(`.xapiand/*` excluded) for the root to reserve in batches; a path in use refuses the whole join,
naming the paths, and releases the rest (nested paths are distinct keys). After the commit the node
drops its old CA, tokens and key and re-founds each shard as a lone-voter group of the new cluster
from its last checkpoint, copying no bytes (strata must add this), with a marker per shard so a
crash resumes; placement then grows each index to its `number_of_replicas` (D7).

At an estimated 100 B of root state per one-shard index and at least 1,000 creations a second
(`cluster-release.md:217`, `:221`), 10,000 indexes take 1 MB and at most 10 s of root commits;
replication copies the carried bytes `number_of_replicas` times, 20 GB for 10 GB at 2 replicas,
160 s at 1 Gb/s. Carried indexes are unavailable until their groups reopen. Joining only when empty
strands the data behind a dump and restore; carrying (Q2) adds only the collision check and the
founding.
Criteria: a cluster of one holding `a` and `b/c` joins 3 nodes, both reading equal everywhere; a
collision on `a` is refused naming it, both sides unchanged; one with a live token is refused; a
kill at each carry step restarts to one whole state (a crash-point job).

**M4. Tokens and the join exchange.** A token is one string, consensus's codec version 2: the
cluster id, which picks its cluster's announcements (`seeds.md:18`); the SHA-256 of the CA's public
key (SPKI, as kubeadm pins), which survives a reissued CA certificate where D150's certificate pin
would not; and a 32-byte secret. The root keeps only the secret's hash, so its backups hold no
usable token, and the secret is printed once, never logged. `POST`, `GET` and
`DELETE /:cluster/tokens[/<id>]` mint, list and revoke; tokens coexist, and rotation is mint, then
revoke.

Any member serves joins on its cluster port to TLS clients without a certificate. The joiner checks
the member's chain against the token's pin before sending the secret, as consensus's `join` does;
the member relays to the root leader, where one command checks token and name, allocates the id,
records the pin and counts the use; the leader then signs, setting its own extensions and ignoring
the request's. A retry with a pinned key gets the same node, so a lost answer burns no use. This
settles TS-R7's anchor, single use and request policy.
Criteria: each refusal (`token`, `expired`, `revoked`, `used`, `name`, `cluster`) beside an accepted
join; a one-use token at two members admits one node; a root leader killed between commit and answer
gives the retry its first id; no log holds a secret.

**M5. Pins live in the root's directory.** Each node applies the directory to its transport with
consensus's `admit_node(node, address, pin)` and `remove_node(node)` (`host_transport.h:341-348`,
node-admission branch), besides verifying chains against the CA bundle; the keepalive answer
carries the directory's version (`src/cluster/rpc_methods.h:140-146`), and a node behind fetches
the changes. This refines D149: the pin is recorded at first join, but in the root, so no peer
trusts on first use and a CA-signed certificate for an unknown id is refused. Pins hash the key
(consensus addition 4), so renewals keep them, which meets the transport-security review's
objection to leaf pins (its answer 2). A new node may be refused for up to one keepalive (500 ms).
Criteria: a retired node is refused, its links closed, everywhere within 2 s; a mutant trusting
first use fails.

**M6. Leaving, dead nodes and removal.** Replicas move automatically; identities never do.

| Event | The node's replicas | Its identity |
|---|---|---|
| `xapiand leave` on it | drained by D7's steps after its leaderships and any root replica, with the CA key, are handed off | retired, its directory deleted (Q6) |
| `down` past `replace_after` | replaced | kept: it rejoins as itself |
| `xapiand remove NODE` | lost with it; a group left without a quorum of unretired voters waits for `recover` (M9) | retired; an `up` node needs `--force` |

`leave` refuses up front when the remaining nodes cannot hold the root or an index's
`number_of_replicas`, naming the indexes (Q5), and prints groups and bytes left, as
`GET /:nodes/<n>` does. Its state is the root's record (`leaving`), so it survives interruption: a
killed command leaves it running (`leave --cancel` undoes it), a restart resumes the drain, a node
dying while leaving retires once its replicas are replaced, and a crash after retirement finishes
the deletion at the next start. `replace_after` is a cluster setting in the root (default 300 s,
changed at runtime); a per-index override is not offered, since raising the cluster's value covers
maintenance with one place to look (Elasticsearch's is per index, 1 min). Retiring revokes
(TS-R5): every node drops the pin (M5), so neither the node nor a copy of its disk connects or
renews again, and the record lasts until the certificate expires, bounding the directory.
Criteria: a leave on 5 nodes under writes keeps digests equal and resumes when killed at each step;
a leave below `number_of_replicas` is refused, naming the index; a removed node's directory copy is
refused; a node down past `replace_after` rejoins as itself.

**M7. Copies, restores and backups.** The keepalive carries a boot id, random per process start
(`rpc_methods.h:131-139`), and an admission number the node raises and persists at each start and
hourly; the root leader records it and answers a second live boot `duplicate` and a number not
above its record `stale`, stopping that process. A node opens its groups only once admitted (root
replicas open the root first), so a copy is refused beside its original, or once the original has
started or refreshed since; a directory moved while stopped runs. This changes D5's "a node
restarts without the root". A cluster of one restored from a copy runs. No identity needs a backup
(`join`, CA replacement or `recover` replaces it); members' data is restored by dump.
Criteria: a copy of a running node exits `duplicate` within 2 s, unseen by the original's groups;
one started after its original refreshed exits `stale`.

**M8. Certificates and the CA.** Node certificates last a year (consensus's default) and renew at
two thirds of their life, with jitter, through the join exchange authenticated by pinned keys both
ways, so pins never change and a node with an expired certificate, or offline through a CA
replacement, still renews. The 10-year CA certificate is reissued with its key at two thirds of its
life; both expiries are metrics warning at five sixths (pki's rule). The CA key is a field of the
root's cluster record: the root's replicas hold it, an install moves it, and other nodes relay.
That reads D151's "every voter" as the root's: at least 95 of 100 nodes hold no key.

`POST /:cluster/ca/:replace` makes a new key, or takes an operator's CA as one PEM file (D150),
renews every active node under it, drops the old CA, closes links whose chain no longer verifies,
and revokes every token: one procedure for a compromise (after `remove`), a loss and an operator's
CA. Rejected: pki's offline CA (D113 Q5 (b)), which hands an operator the key; the key on every
node, more holders for nothing; escrow (TS-R8), since a lost key is replaced.
Criteria: at a 60 s lifetime every node renews under load, no link lost; a node stopped past expiry
renews; after a replacement, old-CA certificates and tokens are refused; a file scan finds the key
on root replicas only.

**M9. Forced recovery.** `xapiand recover [INDEX] --accept-loss` is the only way past a lost
majority, and never automatic. For an index it forces each group whose unretired voters form no
quorum, so the lost voters must be removed first (M6); for the root (no INDEX) it runs on any node
while most root voters are unreachable. Without `--accept-loss` it prints the plan: per group, the
most advanced survivor by log position and what may be lost, the committed entries it lacks (it
knows its leader's commit index) and any write acknowledged after it last heard that leader, or
everything where no copy survives (an `rf = 1` group whose node was removed). With
it, each membership is forced to its survivor under a new epoch (consensus addition 6), or
re-founded empty, and placement regrows it; writes answered `unknown_outcome` may appear, and
returning former voters are refused by the epoch (`host-transport.md:34`).

The root's recovery loses the commits its survivor lacks: nodes admitted then rejoin, index records
are rewritten from the groups' metadata (strata must record each group's index path), and the node
id counter skips ahead. With every root replica lost, a node's directory copy seeds it and the CA is
replaced (M8).
Criteria: an `rf = 1` index whose node was removed answers 503 naming `recover` until
`recover --accept-loss` re-creates it empty; a group with 2 of 3 voters removed recovers from its
most advanced survivor; with 2 of 3 root voters killed and emptied, recovery regrows the root and
admits a node, refusing a former voter's copy; `recover` refuses while most root voters answer.

**M10. Upgrades.** Master's data directory starts as a cluster of one with its indexes; a
Xapian-era multi-node cluster moves by dump and restore, since no cluster mixes versions
(cluster-release section 10). `--solo` and `--cluster` go (`5a517b052^:src/opts.cc:219-220`,
`:381-382`): a cluster of one opens no port, and `--cluster`'s create-or-join split clusters.
`--discovery-*` stay for multicast. Criteria: master's single-node directory serves its indexes.

**M11. Containers and Kubernetes.** A StatefulSet with volumes runs `xapiand join` in every pod,
`XAPIAND_TOKEN` coming from a Secret with a `--ttl never` token and `--seed` naming the headless
service. Bootstrap: one replica runs `xapiand`, `kubectl exec xapiand-0 -- xapiand invite --ttl
never` fills the Secret, and the command becomes `join` (zero-touch is Q4). Scale-down runs `leave`
in the highest pods before lowering `replicas`, since a preStop hook would drain on every rolling
restart. A pod restarted on an empty volume joins as a new node (`xapiand-3-2`), the old one `down`
until removed; root replicas need volumes.
Criteria: a StatefulSet-shaped harness (empty directories, one token, one DNS seed) scales 3 to 5
to 3 through `leave` and restarts each node empty in turn, green between, no `quorum` write lost.

## What consensus must add

1. Token version 2: the cluster id, and the CA's SPKI pin instead of its DER
   (`admission_join.h:25-60`).
2. An asynchronous issuer hook, idempotent per key, given the secret's hash, the name and the
   request's key, for one secret and a synchronous `assign` (`admission_join.h:185`, `:196`);
   `Joined::save` by rename.
3. The join exchange on the transport's port (today a separate socket, `admission.md`).
4. Node pins over the key's SPKI (today the leaf's DER, `tls.h:36-44`), optional for Detent.
5. Renewal by pinned keys, and runtime certificate and bundle reload closing links whose chain no
   longer verifies (TS-R12).
6. A forced configuration under a new epoch, beside single-step changes (`ARCHITECTURE.md:31`), and
   each replica's log position and last-heard commit index.

strata adds founding from a checkpoint (M3) and index paths in group metadata (M9); pki's planned
signing is not needed, since consensus's `Issuer` signs.

## How others do it

| Case | etcd | CockroachDB | Elasticsearch 8 | kubeadm | This design |
|---|---|---|---|---|---|
| First start | one-member cluster | `start`, then `init` | one-node cluster | `kubeadm init` | cluster of one |
| Joining | `member add`; operator certificates | `--join`; operator CA | token: CA fingerprint, addresses, API key; 30 min | token, 24 h, and the CA key's hash | token: cluster id, CA key pin, secret |
| Dead node | `member remove` | replicas moved after 5 min; `node decommission` | shards moved after 1 min | `kubectl delete node` | replicas moved after `replace_after`; `remove` |
| Another cluster's data | refused | refused | detach, then dangling-index import | none | carried in unless names collide |
| Lost quorum | `--force-new-cluster` | `debug recover` | `elasticsearch-node unsafe-bootstrap` | etcd's | `recover --accept-loss` |

## Limits

1. **Silent:** a lost disk under plain `xapiand` serves its routed clients an empty cluster and
   takes their writes (M2).
2. **Silent:** a copy admitted while its original is cut off from the root, or started after the
   original dies and before its hourly refresh, runs on duplicated or rolled-back state and can vote
   twice in a term (M7).
3. **Silent for clients:** a group recovered while its removed voters live behind a partition runs
   twice until it heals, and what the far side acknowledged is lost; `remove` and `recover` take the
   operator's word that nodes are gone.
4. A root replica's backup holds the CA key (against the transport-security review's answer 3,
   under the trusted-cluster model): it can impersonate the cluster to a joiner and take its secret,
   but admits no node, since pins decide.
5. A node restarting while the root has no quorum waits (M7); below three nodes the root has one
   voter, whose loss needs `recover`.

## Questions for the owner

- **Q1. Token defaults?** Default: 24 hours (kubeadm's), unlimited uses within them, `--ttl never`
  allowed.
- **Q2. Carry a cluster of one's indexes in (M3), or join only when empty?** Default: carry.
- **Q3. Token routes on the unauthenticated API?** Anyone reaching it can already delete every
  index; the routes take the API token once it exists (TS-R9). Default: yes.
- **Q4. Zero-touch bootstrap of identical pods** by a pre-shared secret (kubeadm's bootstrap tokens,
  Elasticsearch's initial master nodes)? Default: not in the release.
- **Q5. Downsizing below `number_of_replicas`: refuse, or warn and lower it?** Lowering changes
  users' durability, and running short (D7) leaves each group one voter. Default: refuse, naming the
  indexes.
- **Q6. `leave` deleting the drained node's data?** Its root replica, with the CA key, goes either
  way. Default: yes.

## Status and first steps

The node-admission branch holds consensus's version 1 token join, unlanded. Steps: consensus
additions 1 to 4; `membership` (M1, M2, M4 to M7); renewal and CA replacement (M8); `recover` (M9);
the carry (M3). The lead's criteria change: A4 drops loopback plaintext (D150), A7 and A8 found at
first start and mint with `invite`, and A10's token must be live (Q1). Review first: M5's trust
root, M7 against D5's restart property, M9's survivor choice, M3's crash points, Limits 2 and 3.
