# Cluster membership

How nodes get identities, join, leave, and survive lost disks and nodes, copied directories,
expiring certificates and a lost quorum, with no key ever handled by an operator.

- Status: revision 3, 2026-11-20, for the last design round (lens `design`); nothing is built. It
  folds the re-check's F17 to F29 (`.campaign/xapiand/reviews/cluster-membership-design.md`) and
  D158's noun-verb CLI into D154 and D157. Read at Xapiand `6e7ae7926`, consensus `e1f249d` and
  `64dbb06`, and pki `70a3b76`, under the trusted-cluster threat model.
- Models: `docs/design/cluster-membership/m7_admission.py` (job `261120-132700-b74ba7`, at
  `47d438113`) and `m9_survivor.py` (job `261120-125810-f56d46`, unchanged since).

## Abstract

`xapiand [--token TOKEN]` runs a node, and its data directory decides what the token does: an empty
one founds a cluster of one without a token and joins with one. After the first `token create`,
every node, the founder included, runs the same command. A dead node's replicas move automatically;
identities go only by `node leave` or `node remove`, data only by `salvage --accept-loss`. Trust
lives in the root group's node directory, not in the certificate authority (CA), whose key only the
root's replicas hold.

## Commands and directory states

```text
xapiand [--token TOKEN]
xapiand token create [--ttl D|never] [--uses N] | token list | token revoke ID
xapiand node list | node leave | node remove NODE [--force]
xapiand cluster health | cluster salvage [--accept-loss]
xapiand index salvage PATTERN [--accept-loss]
```

Only the first runs a node, preferring `XAPIAND_TOKEN`, since other local users can read arguments.
The others are one-shot calls to its `/:cluster/` routes on the local socket (M4): tokens (rotation
is create, then revoke), this node's leave and another's removal (M6), and health and salvage of the
root or index groups (M9); `node leave` streams its drain. `invite` and `recover` go (D158).

| The data directory holds | Without a token | With a token |
|---|---|---|
| nothing | founds a cluster of one | joins, never founding |
| a saved key and no identity | refused, naming the token | joins with the key (M4) |
| a cluster of one that never had a second node and has no live token | runs | joins, carrying its indexes (M3) |
| a member of the token's cluster | runs | runs, ignoring the token |
| another cluster's member otherwise, a cluster of one with a live token included | runs | refused (`cluster`), naming both ids |
| a retired marker (M6) | refused (`retired`) | the same |
| a duplicate, stale or unknown identity | refused, naming which and what to do (M7) | the same |

## Decisions

**M1. Identities.** A cluster is a 16-byte random id (the cluster field of consensus's hello,
`host-connection.md:99`), a name and a P-256 CA. A node is an id from the root's counter, never
reused; a P-256 key that never leaves it; a certificate for
`spiffe://<cluster id>/xapiand/node/<id>`; and a name unique among unretired nodes, by default the
host name. The root's records, the contract for `xapiand:node-directory`:

| Record | Fields |
|---|---|
| Cluster | id, name, CA certificates and key, next node id, next admission number, directory version |
| Node | id, name, key pin, certificate expiry, addresses, `active`, `leaving` or `retired`, admitted boot, admission number, pending offer and its boot (M7) |
| Token | id, the secret's SHA-256, expiry, uses left |

Two first starts found two clusters, so nothing races. An explicit `--name` held by an unretired
node is refused (`name`, TS-R2); a default one takes a suffix (`xapiand-3-2`). pki's Xapiand
identity must name the id, not the name (`pki/include/pki/identity.h:30-33`).
Criteria: cluster A8, with first starts in place of `init`.

**M2. A cluster of one, and the token on the run command.** `xapiand token create` mints a token
(M4) and prints the run command seeded at this node, first opening a cluster of one's cluster port
(default 9880, `5a517b052^:config.h.in:285`) and announcements. With a token, the run command reads
the directory first, so a member ignores the token and never waits on it; an empty directory finds
its cluster by multicast or `--seed HOST[:PORT]`, retries until it answers (cluster A9), joins and
runs; an expired, revoked or used token exits nonzero naming it, never founding.

Every node's deployment runs the same command with the token, the founder's after its first
`token create` (it is a member of the token's cluster, F26), so a lost disk rejoins as a new node
and nothing founds by accident. Only a node started without a token founds, which splits no
membership (TS-R6): it neither announces nor holds a token, and old peers refuse its certificate
(Limit 1). Founding warns, and `GET /` shows the node count.
Criteria: `xapiand` without a token binds only the HTTP port and sends no datagram for 10 s; after
`token create`, a second empty directory with the token serves, and restarts after the token
expires; the founder restarts with its own live token; an empty one with an expired token exits
`expired`.

**M3. Carrying a cluster of one's indexes in.** The join lists the node's index paths
(`.xapiand/*` excluded) for the root to reserve; a path in use refuses the whole join, naming the
paths (nested paths are distinct keys). After the commit the node drops its old CA, tokens and key,
closes each group (a close checkpoints the applied position, `cluster-release.md:303`) and
re-founds it from that checkpoint as a lone-voter group of the new cluster, copying no bytes
(strata must add this), with a marker per shard so a crash resumes; placement then grows each index
to its `number_of_replicas` (D7). Carrying, rather than joining only when empty, is the owner's
(D154). At 1,000 creations a second (`cluster-release.md:221`), 10,000 indexes take 10 s of root
commits.
Criteria: a cluster of one holding `a` and `b/c` joins 3 nodes, both reading equal everywhere; a
collision on `a` is refused naming it, both sides unchanged; one with a live token is refused; a
kill at each carry step restarts to one whole state (a crash-point job).

**M4. Tokens, the join exchange and the local socket.** A token is one string, consensus's codec
version 2: the cluster id, which picks its cluster's announcements (`seeds.md:18`); the SHA-256 of
the CA's public key (SPKI, as kubeadm pins); and a 32-byte secret, printed once and never logged,
whose hash alone the root keeps. A token lives 24 hours by default (kubeadm's) and is reusable within
them, with `--ttl never` allowed (D154). Until API tokens exist (TS-R9), the membership routes are
served only on a Unix socket in the data directory (`-D`, `src/opts.cc:285`), mode 0600, never on
HTTP, where a local proxy or sidecar would forward remote requests from loopback (F28).
`/:cluster/health`, which changes nothing, is on HTTP too.

Any member serves joins on its cluster port to TLS clients without a certificate, in their own
pool: at most 64 at once and 8 per source, refused past the cap rather than evicting links, a source
opening more than 10 a second delayed, and a slot closed 10 s after its accept unless its handshake
and request arrived (F25). The joiner saves its key before its first request and sends the secret
only to a member whose chain matches the token's pin; the member relays to the root leader, where
one command checks token and name, allocates the id, records the pin and counts the use, and the
leader signs with the CA key current at that commit, setting its own extensions. A retry with the
saved key, after a lost answer or a crash, gets the same node and burns no use.
Criteria: each refusal (`token`, `expired`, `revoked`, `used`, `name`, `cluster`) beside an accepted
join; a one-use token at two members admits one node; a root leader or a joiner killed after the
commit retries as the first id, the use kept; 64 concurrent joins at one member while links redial
drop no link handshake, and 64 idle sockets free their slots in 10 s; no log holds a secret;
membership routes answer 404 on HTTP from loopback, and the socket refuses other users.

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
| `node leave` on it | drained by D7's steps after its leaderships and any root replica, with the CA key, are handed off | retired; its data deleted (D154) but for a marker, so a restart with the token exits `retired` until the directory is emptied by hand (F21) |
| `down` past `replace_after` | replaced | kept: it rejoins as itself |
| `node remove NODE` | lost with it; a group left without a quorum of unretired voters is lost until salvaged (M9) | retired; an `up` node needs `--force` |

`node leave` refuses when the remaining nodes cannot hold the root or an index's
`number_of_replicas`, naming the indexes, never lowering it (D154); check and mark are one root
command counting only nodes neither leaving, retired nor down, so concurrent leaves and removes see
each other. The drain is the root's record: a killed `node leave` leaves it running (`--cancel`
undoes it), and a node dying while leaving retires once its replicas are replaced. `replace_after`
is one cluster setting (default 300 s). Retiring revokes (TS-R5): every node drops the pin, so
neither the node nor a copy of its disk connects or renews again.
Criteria: a leave on 5 nodes under writes keeps digests equal and resumes when killed; of two
concurrent leaves on 4 nodes at `rf = 3`, one is refused, naming the index; after a leave, a restart
with the token exits `retired` and an emptied directory joins as a new id; a removed node's directory
copy exits `retired`; a node down past `replace_after` rejoins as itself.

**M7. Copies, restores and backups.** Each process start is a boot, a random id
(`rpc_methods.h:134`). The hello carries the boot and the directory version that admitted it
(consensus addition 7); every acceptor, root replicas included, refuses any other boot with
`duplicate` while the admitted one was heard within 2 s, fetching first when behind that version. A
clean stop clears the boot. A new root leader counts the 2 s from its election; a running node asks
a root replica for the leader at each missed 500 ms keepalive (`rpc_methods.h:131`), with 500 ms
deadlines on lookup and dial, so it arrives within 1.5 s even when the old leader's host died. A
starting process retries `duplicate` for 2.5 s from the first, at any hello before admission, since
the live boot may be its own crashed predecessor.

The run command admits its boot at startup, after any join, in three steps. The boot presents its
saved admission number: below the node's record it is `stale`, and any other gets a committed fresh
offer above both. The node saves the offer and presents it; the root commits it as the record with
the boot, and only then does the node open groups. Only the boot an offer was made to completes it,
so every directory older than the last confirm holds less than the record. A running boot refreshes
the same way hourly: 2 root commits per node, 0.6 a second per 1,000 nodes.

A number above the record means the record lags (F17): after a root salvage lost a node's refresh,
the three steps re-admit it with its data, and a node whose join was lost likewise, under the id its
certificate proves (M9). With no root leader, root voters, which open the root group only once
admitted, check each other against their own copies: `stale` below the record, accepted at it or
the pending offer, refused until a leader exists above it. A majority of copies holds every commit
and accepts every voter, so it elects a leader, which lagging voters dial to catch up and which
admits every voter by the three steps before serving (changing D5's "a node restarts without the
root").

A refused node exits naming what to do: `duplicate`, another process runs this identity; `stale` (an
older copy) or `unknown` (a certificate the bundle no longer verifies), empty the directory and the
token rejoins it as a new node; `retired`, M6. A node whose handshakes fail its pin asks the join
exchange by its key, which re-admits it or answers `retired` or `unknown`.

In the model, revision 1's rule (the record or the pending offer, by any boot) admits a directory
after a sibling confirmed 1,618 times in 4,000 random histories and locks the newest out in 90
(2,241 with salvages), answers a salvage's lost refresh `stale` and lost join `unknown`, and exits a
voter in 23,040 of 33,280 three-voter cold starts; this rule does none of these, nor in 20,000
five-voter ones. Over 40,000 root elections the original arrives within 1.47 s and its hot copy is
never admitted (519 times with a 1.5 s dial). A backup older than one refresh still reads `stale` on
1,000 restarts, a cold copy that ran makes its original `stale`, and a kill at any join or admission
step ends admitted as one id, the use kept.
Criteria: a hot copy beside a running original exits `duplicate` within 3 s of its first hello and
opens no group, the root's included, across 20 root leader changes, each killing the leader's host;
a backup older than one refresh exits `stale` on each of 100 restarts; a kill between steps,
restarted at once, ends admitted; a cold copy that ran makes its original `stale`; a cold start with
one voter's copy behind admits every voter; after a root salvage that lost one node's refresh and
another's join, both rejoin with their data.

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

**M9. Health and salvage.** `xapiand cluster health` prints `/:cluster/health`: red when a group
lacks a quorum, yellow when one is under-replicated or a node is down or leaving, else green; each
node up, down since when, leaving or retired; and the groups worst first: lost (no quorum of
unretired voters, naming the salvage command), no quorum while waiting on named down nodes, and
under-replicated. `node remove` declares a node gone for good, turning a waiting group lost:

```text
xapiand cluster health        # red: twitter shard 3 no quorum, waiting on 4 and 7, down since 09:12
xapiand node remove 4; xapiand node remove 7
xapiand cluster health        # red: twitter shard 3 lost: xapiand index salvage twitter --accept-loss
xapiand index salvage twitter --accept-loss
```

Salvage is never automatic: `index salvage PATTERN` takes each lost group of the indexes PATTERN
matches (A3), `cluster salvage` the root while most root voters are unreachable. Among the survivors
holding every entry up to the highest commit index any survivor heard, it keeps the most up to date
as a vote compares (last term, then last index). In the model, the longest log loses an entry
another survivor heard committed in 239 of 40,000 five-voter survivor pairs, the vote's comparison
in 3, this rule in none, nor in 35,000 seven-voter triples. Without `--accept-loss` it prints the
plan per group: the survivor kept, the entries other survivors hold that it lacks, and that entries
only the lost voters held are lost unlisted (1,827 of those pairs). With it, each membership is
forced to its survivor under a new epoch (consensus addition 6), or re-founded empty, and placement
regrows it; writes answered `unknown_outcome` may appear, and the epoch refuses returning former
voters (`host-transport.md:34`).

A root salvage loses the commits its survivor lacks: nodes whose refresh or join it lost are
re-admitted (M7), index records are rebuilt from the groups' metadata (strata must record each
group's index path), and the id counter skips past the highest id any reachable node or group
membership names, so only a node holding no replica can see its id reissued (it reads `unknown`).
With every root replica lost, a node's directory copy seeds the root and the CA is replaced (M8).
Criteria: the model's traces `trace_vote` and `trace_index` as tests of the forced configuration;
an `rf = 1` index whose node was removed shows lost and answers 503 naming `index salvage` until
salvaged empty; with 2 of 3 root voters killed and emptied, `cluster salvage` regrows the root and
admits a node, refusing a former voter's copy, and refuses while most root voters answer; `cluster
health` names each state in a harness producing it.

**M10. Upgrades.** Master's data directory starts as a cluster of one with its indexes; a Xapian-era
multi-node cluster moves by dump and restore. `--solo` and `--cluster` go
(`5a517b052^:src/opts.cc:219-220`, `:381-382`).
Criteria: master's single-node directory serves its indexes.

**M11. Containers and Kubernetes.** A StatefulSet with volumes runs `xapiand` in every pod, with
`XAPIAND_TOKEN` from an optional Secret holding a `--ttl never` token and `--seed` naming the
headless service. Bootstrap: `xapiand-0` starts with the Secret empty and founds, `token create
--ttl never` there fills the Secret, and the other pods join; `xapiand-0`, a member, ignores the
token from then on (no zero-touch bootstrap in the first release, D154). Scale-down runs `node
leave` in the highest pods, whose restarts then exit `retired`, before lowering `replicas`; a
preStop hook would drain on every rolling restart.
Criteria: a StatefulSet-shaped harness (empty directories, one token, one DNS seed) scales 3 to 5
to 3 through `node leave` and restarts each node empty in turn, green between, no `quorum` write
lost.

## What consensus must add

1. Token version 2: the cluster id, and the CA's SPKI pin instead of its DER
   (`admission_join.h:25-60`).
2. An asynchronous issuer hook, idempotent per key (`admission_join.h:185`, `:196`); `join` taking a
   key its caller saved, where today it makes one per call (`admission_join.h:338`); `Joined::save`
   by rename.
3. The join exchange on the transport's port (today a separate socket, `admission.md`), with its own
   pool, rate and slot deadline (M4) outside `handshakes` (`host-connection.md:136-145`).
4. Node pins over the key's SPKI (today the leaf's DER, `tls.h:36-44`).
5. Renewal by pinned keys, and runtime certificate and bundle reload closing links whose chain no
   longer verifies (TS-R12); the hello path re-checks chain and expiry, since the port also takes
   clients without valid certificates.
6. A forced configuration under a new epoch (`ARCHITECTURE.md:31`), and each replica's last term,
   last index, heard commit index, term at an index and entries past one (F23).
7. The boot and the directory version that admitted it in the hello, and a reason in its refusal,
   checked by every acceptor against the directory's admitted boot (M7).

strata adds founding from a checkpoint (M3) and index paths in group metadata (M9).

## How others do it

| Case | etcd | CockroachDB | Elasticsearch 8 | kubeadm | This design |
|---|---|---|---|---|---|
| Joining | `member add`; operator certificates | `--join`; operator CA | token: CA fingerprint, addresses, API key; 30 min | token, 24 h, and the CA key's hash | token: cluster id, CA key pin, secret |
| Dead node | `member remove` | replicas moved after 5 min | shards moved after 1 min | `kubectl delete node` | replicas moved after `replace_after`; `node remove` |
| Lost quorum | `--force-new-cluster` | `debug recover` | `elasticsearch-node unsafe-bootstrap` | etcd's | `cluster salvage`, `index salvage` |

## Limits

1. **Silent:** a node started without a token on a lost disk founds, serving routed clients an
   empty cluster (M2).
2. **Silent:** a copy admitted while its original is cut off from the root past 2 s, a backup taken
   since its node's last refresh and restored after the node died, or a root replica copied or
   restored and started while the root has no leader runs on duplicated or rolled-back state and
   can vote twice in a term; the last also rolls the root back: ids reissued, retired pins back,
   token uses reset (M7).
3. **Silent for clients:** a group salvaged while its removed voters live behind a partition runs
   twice until it heals, losing what the far side acknowledged; `node remove` takes the operator's
   word. After a root salvage, a backup from the lost commits, started first, is admitted beside its
   node (S10).
4. A root replica's backup holds the CA key: it can impersonate the cluster to a joiner and take
   its secret, so it admits no node without a live token.
5. A node restarting while the root has no quorum waits (M7); below three nodes the root has one
   voter, whose loss needs `cluster salvage`.
6. Until API tokens exist, membership is managed only on a node's host, by its data directory's
   owner or root (M4); a node so admitted may become a root voter holding the CA key.

## Review findings

F1 to F16 are closed (the re-check); F11's single SPKI pin waits on Detent's lead. F17 is M7's rule
(S9 to S11). F19 is M7's window and criterion (S12); F20 and F26 the directory table; F21 (M6) and
F28 (M4, Limit 6) the lead's defaults; F22 Limit 2; F23 and F24 additions 6 and 7; F25 M4's slot
deadline; F27 M7's retry and S8's count from the first hello; F29 the dead-node row and the limits'
order.

## Questions for the owner

None open: D154, D157 and D158 settled them; F21 and F28 stand as the lead's defaults unless the
owner overrides them.

## Status and first steps

Steps: consensus additions 1 to 5 and 7 (node-admission holds version 1); `membership` (M1, M2, M4
to M7, the socket); renewal and CA replacement (M8); health and salvage with addition 6 (M9); the
carry (M3). The lead's criteria changes:

1. A4 drops loopback plaintext (D150) and `--join`.
2. A7 and A8: an empty directory founds without a token, `token create` mints, and `init` and the
   empty-directory refusal go (D157, D158).
3. A9 is the run command with a token: never founding, a member ignoring it, an expired one named.
4. A10's token must be live (24 h, D154), the new node admitted through any member.
5. New: M7's, with the cold start, the root salvage and the leader's host killed; M9's; M4's joins,
   idle slots, joiner crash and socket; M6's concurrent leaves and retired marker.

Review first: M7's `stale` below the record and the cold root, M4's socket, M9's health states.
