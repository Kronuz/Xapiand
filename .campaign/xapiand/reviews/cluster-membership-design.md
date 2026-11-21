# cluster-membership review: design

Commit: Xapiand:d3c21746b492a990dfd402618c7192bea3baf27e (`docs/design/cluster-membership.md`, revision 0)
Verdict: CHANGES
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right and meets the owner's directions: one TLS path, no key in an operator's hands,
plain `xapiand` as a cluster of one promoted by `invite`, a `join` that keeps running, trust in the
root's directory rather than in the CA, and identities that leave only by `leave`, `remove` or
`recover`. Three decisions fail as written, and the design names all three as review targets. M7's
admission number lets a rolled-back directory in after enough restarts (F1). Its `duplicate` can
neither reach a copy nor outlive a root leader change (F2). M9's survivor choice can drop writes
that another survivor heard were committed (F3). Each fix is a rule and a criterion in the
document, not a new approach.

## Evidence I relied on

No quest job covers this design: A5's job (`261120-104107-5a4591`) proves the shard RPC, not
membership. I read the document's pins:

1. Consensus `e1f249d`: `ARCHITECTURE.md`, `docs/mechanisms/host-connection.md` and `host-transport.md`.
2. Consensus `64dbb06`, the node-admission branch: `admission_join.h`, `node_pins.h`, `host_transport.h` and `docs/mechanisms/admission.md`.
3. Xapiand at the reviewed commit: `src/cluster/rpc_methods.h`, `docs/mechanisms/shard-rpc.md` and `docs/design/cluster-release.md`.
4. The quest files `cluster.md`, `node-admission.md` and `placement.md`, and PLAN.md D149 to D152.

## What I checked myself

Two models settle the disputed behavior. Their scripts are under
`.scratch/xapiand/review-cluster-design/` in the home checkout, and both exit 0 when their
expected outcome holds. Both jobs below passed. Their seeds are fixed, so the copies tagged
`cluster` (`261120-121534-0c2bc4`, `261120-121535-7509bb`), queued behind the quest's build-test,
reproduce the same lines.

1. **`m7_admission.py`, job `261120-121643-c02f08`.** M7 under the design's rule and under one
   candidate fix. The model reads the design charitably: the admitted boot is committed with the
   number, so a running node's own keepalives pass after a root leader change. The transport's
   rule is in it: one undrained lane per node, a second hello refused.
   - S1: a backup 30 refreshes old is admitted on its 31st supervisor restart.
   - S3: a cold copy runs first; its original crash-loops 200 times and is then admitted at the
     copy's first lane drop, holding older state than the copy.
   - S4: at a root leader change, a hot copy is admitted beside its running original in 49.4% of
     2,000 trials.
   - The fix: the root commits the number it offers, and the node persists it only after that
     commit. It refuses S1 on every one of 1,000 restarts and refuses S3. A crash between the offer
     and the persist does not lock the node out (S2). Admission alone still loses S4 (51.2%). With
     the boot in the hello, refused by every acceptor, S4 reaches 0%.
2. **`m9_survivor.py`, job `261120-121646-83aa8f`.** Five voters under consensus's rules: a vote
   compares last term, then last index; a leader appends a no-op on election; a leader commits
   only a current-term entry that a majority matched. It runs two scripted traces, then 4,000
   random schedules. Every pair of survivors is scored on whether the chosen survivor lacks an
   entry that another survivor heard was committed.
   - By log index, it does in 239 of 40,000 pairs.
   - By a vote's comparison, it does in 3.
   - Among the survivors that hold everything up to the highest commit index any survivor heard,
     choosing by the vote's comparison, it never does. 6 pairs still lose a commit that no
     survivor heard of.
   - The model's own safety check never fires: no two committed entries differ at one index.

**The six consensus additions, and the one the design misses.**

| Addition | Buildable on consensus as it is | Sound | Finding |
|---|---|---|---|
| 1 Token v2: cluster id, CA SPKI pin | yes; `JoinToken` and its codec change | yes | |
| 2 Asynchronous, idempotent issuer | yes | yes, once `join` takes a persisted key | F4 |
| 3 Join exchange on the transport's port | yes, with a dispatch before the hello | needs its own pre-auth bound | F8 |
| 4 SPKI node pins | yes | yes; two pin kinds if Detent keeps DER | F11 |
| 5 Renewal by pinned keys, runtime reload | yes | yes; expired-certificate leniency must stay off the hello path | F8 |
| 6 Forced configuration under a new epoch | yes: the epoch is the open frame's 16 bytes, compared for equality (`host-transport.md:34`, `:67-71`), so two recoveries with fresh epochs refuse each other | the survivor rule is not | F3 |
| missing: the boot in the hello | needs a hello field | required by M7's own criterion | F2 |

**Security.** A token is a bearer secret for its TTL, and theft or replay inside that window
admits a node, as with kubeadm and Elasticsearch. The root stores only the secret's hash. The
joiner sends the secret only after the CA pin checks out. Members relay the secret, which is
acceptable under the trusted-cluster model. Pins come from the root's records with no
trust-on-first-use, and retirement drops a node's pin everywhere. NodePins' removal epoch closes
the gap between a handshake and a removal. The CA key lives in root replicas, their backups
(Limit 4) and replicas that have moved away (F14). Limit 4 overstates what a key holder cannot
do (F9).

**Operator experience.** The surface is close to minimal: six verbs, and one run command (`join
TOKEN`) for every node once a cluster exists. There is one way to add a node, and the two ways to
take one out fit two different situations. Three wrinkles remain:

1. The founder's run command has to be switched by hand (F12).
2. Tokens and CA replacement exist only as HTTP routes, while the other actions are commands.
3. `recover` handles one index at a time (F16).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Each start raises M7's admission number, refused or not, so `stale` refuses only one start. A supervisor's restarts lift a restored backup's number past the record. A refused original crash-looping beside a cold copy is admitted, with older state than the copy, at the copy's first lane drop or root leader change. Either way, every group the node votes in rolls back, which Raft does not survive. | `cluster-membership.md:146-149`; m7 S1 (31 restarts), S3, job `261120-121643-c02f08` | The number advances only after the root commits it. For example, the root commits the next number it offers, the node persists it after that commit, and a new boot is admitted when it presents the recorded or the offered number. Criteria: a directory restored from a backup older than one refresh exits `stale` on each of 100 restarts, and a kill between the offer and the persist restarts admitted (m7 S1 and S2 under `offered`) |
| F2 | yes | gap | `duplicate` cannot reach a copy and does not survive a root leader change. The keepalive rides the pair's Bulk link (`shard-rpc.md:3-4`), and an acceptor refuses a hello for a lane it already holds (consensus `host-connection.md:121`). A copy beside a linked original therefore never hears `duplicate`, so the criterion "exits `duplicate` within 2 s" cannot pass. Boot liveness lives only in the leader's keepalive state; the Node record (`:53`) has no boot. At a root leader change, whichever process dials first is admitted, and the original, its lane taken, loses the root. Root replicas open the root group before admission (`:149-150`), which exposes the root itself. Limit 2 names neither case. If instead each new leader checked the number of each boot's first keepalive, every running node would be `stale` at a leader change. The design must commit the admitted boot. | m7 S4: the copy is admitted in 49.4% of leader changes, and in 0% once the boot is in the hello; job `261120-121643-c02f08` | Option (a): consensus addition 7. The hello carries the boot, the root's record commits the admitted boot, and every acceptor refuses another boot while the admitted one is live; this covers the root group too. Option (b): keep the mechanism, widen Limit 2 to name root leader changes and root replicas, and drop the 2 s criterion. Criterion for (a): a hot copy started beside a running original never opens a group, the root's included, across 20 root leader changes |
| F3 | yes | defect | M9's "most advanced survivor by log position" can drop writes that another survivor heard were committed. By log index, an old leader's long uncommitted suffix wins over committed newer-term entries. By a vote's comparison, a survivor holding only its own newer-term no-op wins over one that heard an older-term entry committed. The plan also reports only what the chosen survivor's own leader committed (`:178-179`), so it under-states the loss. | m9 `trace_index` (2 entries lost), `trace_vote` (1), explorer 239 and 3 of 40,000 pairs; job `261120-121646-83aa8f` | Choose among the survivors that hold every entry up to the highest commit index any survivor heard, then by the vote's comparison. The plan lists the entries other survivors hold that the chosen one lacks. Addition 6 exposes each replica's last term, last index and heard commit index. Criterion: both traces as tests of the forced configuration |
| F4 | no | gap | M4's "a retry with a pinned key gets the same node" needs the joiner's key to survive the joiner's own crash. Today `join()` generates a fresh key on every call, and `save` writes it only after success. A joiner killed after the root's commit comes back as a new node: the phantom keeps the name, and a one-use token answers `used`. | consensus `64dbb06:admission_join.h:338`, `:314` | Addition 2: `join` takes a key the caller persists before its first attempt. Criterion: a joiner killed between the root's commit and its save restarts as the same node, and the token keeps its use |
| F5 | no | gap | `leave`'s up-front refusal (`:132`) races another `leave`. On 4 nodes at `rf = 3`, two concurrent leaves each count three remaining nodes, and the drains leave two. | `:132-136` | Make the check and the `leaving` mark one root command, counting only nodes that are neither leaving, retired nor down. Criterion: of two concurrent leaves, one is refused |
| F6 | no | gap | Nothing tells a node it is retired or unknown. Refusals come before any frame (`admission.md`, Refusals), and a TLS alert carries no reason. A retired node whose deletion was interrupted (`:136-137`) and a node whose admission a root recovery lost (`:185`) both "run; the token goes unused" (`:37`) and stay refused for good. "Skips ahead" (`:187`) names no amount. | `:37`, `:136-137`, `:185-187` | The pinned-key exchange answers `retired` (the node finishes its deletion) or `unknown` (`join` falls back to its token as a new id). Name the skip's bound, or derive it from the highest id any reachable node knows |
| F7 | no | gap | Each join is committed and then signed (`:105-106`). A CA replacement that lands between the two signs with a key the joiner did not pin: the joiner answers `issued` and its use is counted. After the replacement, a revoked token fails at the joiner as `ca`, before the root can answer `revoked`. | `:105-106`, `:165-167` | State which key signs: the one current at the join's commit. The replacement waits for in-flight joins. The joiner's message for `ca` names a possible CA replacement |
| F8 | no | gap | Addition 3 names no bound of its own, so joins on the transport's port would fall into its pre-auth pool: 4 handshakes, 2 per source, a newcomer evicting the oldest. A join holds its slot through a root commit, so during a mass join (a StatefulSet scaling up) joiners and link handshakes would evict each other. A port that accepts cert-less clients (joins) and expired ones (renewal) moves the chain and expiry checks of the hello path from TLS into code. | consensus `host-connection.md:136-145`; `:157-160`, `:217` | Give the join exchange its own bound outside `handshakes`. The hello path re-checks chain and expiry. Criterion: 64 concurrent joins at one member while links redial, with no link handshake dropped |
| F9 | no | defect | Limit 4 says a holder of a backed-up CA key "admits no node, since pins decide" (`:249`). With a live multi-use token that is false: the key holder impersonates a member to a joiner, takes its secret, and joins through a real member, which records the attacker's key. | `:247-249`, `:103-107` | "Admits no node without a live token" |
| F10 | no | nit | Q3 weighs the token routes against index deletion (`:258-259`). A node joined through an HTTP-minted token can also be placed as a root voter and receive the CA key. | `:162`, `:258` | Add that to the owner's question |
| F11 | no | nit | "Optional for Detent" (`:218`) leaves two pin kinds in consensus. | `:218` | Use one SPKI pin for both consumers, agreed with Detent's lead |
| F12 | no | nit | Only the Kubernetes path (`:203`) moves the founder's run command to `join`. On bare metal, a founder kept on plain `xapiand` that loses its disk founds again silently (Limit 1). | `:62-74`, `:201-203`, `:239-240` | The owner decides. Options: `invite` also prints the founder's own `join` line (recommended); or keep the behavior as Limit 1 states it |
| F13 | no | nit | M3 re-founds a carried group "from its last checkpoint". That is safe only if the carry first closes each group, since a close checkpoints the applied position (`cluster-release.md:303`). Refusing a cluster of one that once had a second node (`:36`) is unjustified, against "a solo node can later join". | `:36`, `:81-82` | Say the carry closes each group first. Justify the condition, or let any cluster of one without a live token carry |
| F14 | no | nit | M8's key scan does not cover a root replica that moved away, or the cluster record served by a route. | `:162`, `:171-172` | Run the scan after a root replica moves, and test that no route or log carries the key |
| F15 | no | nit | Nothing ties M7's hourly refresh to a mechanism, yet it sets Limit 2's rollback window. | `:148`, `:242` | Name the refresh's cost (root commits per node) and the window it buys |
| F16 | no | nit | `recover INDEX` works one index at a time. | `:174` | Accept a pattern over the root's records (A3) |

## Not run

Nothing is built, so no criterion was exercised. The models abstract the transport and Raft. They
leave out pre-vote and the leader-contact guard, which affect only availability; the scripted
traces remain reachable with both. I did not check the scale of keepalives and directory fetches
past 100 nodes, the Kubernetes harness, or the upgrade path (M10).

## Re-check (revision 1)

Commit: Xapiand:8e68657230b95d52fec72cc2c59f4d6ec7d78d20 (`docs/design/cluster-membership.md`, revision 2, which landed during the re-check and folds D157; the models are those of `16176b53a`, unchanged since)
Verdict: CHANGES
Date: 2026-11-20

Revisions 1 and 2 close F1 to F16, and the models back the rules they model. One defect remains,
in M7's `stale`: it is decided by equality with a record that can lag the node, at a cold start
against a lagging root voter's copy, and after a root recovery that lost the node's last refresh.
Both tell the operator to empty a directory holding the newest state (F17). Revision 2's 2.5 s
`duplicate` retry fixes the immediate-restart lockout I would otherwise have raised, admits no copy
the 2 s window did not already admit, and keeps a hot copy inside the 3 s criterion.

### Evidence I relied on

1. **M7, job `261120-125807-e2b893`** (passed at `16176b53a`, whose models equal HEAD's): every
   check `ok`. It reproduces the first review's lines (S1 at 31 restarts, S4 at 49.4% and 51.2%),
   then revision 1's (S5 `stale`, S1 `{'stale'}` over 1,000 restarts, S2 `admitted` at four kill
   points, S4 0 of 2,000 trials of 20 leader changes) and revision 2's (S6 restarts 0 to 6 s after
   a kill at each admission step end `admitted` as one id with the use kept, and `duplicate`
   without the retry; S7 the same at six first-start kill points; S8 a hot copy exits `duplicate`
   within 3 s).
2. **M9, job `261120-125810-f56d46`**: every check `ok`. Over 40,000 five-voter pairs, a known loss
   in 239 by index, 3 by vote, 0 by `known`; over 35,000 seven-voter triples, 159, 0 and 0. Under
   `known`, 6 and 1 sets still lack a committed entry another survivor holds, which the plan lists,
   and 1,827 and 298 lose one no survivor holds.
3. The rev-1 jobs at `48d014024` (`261120-124308-b19246`, `261120-124310-76ee22`) were cancelled
   when revision 2's replaced them; two I queued at that commit I cancelled as moot.

### What I checked myself

1. **F1's claim.** The model's `offered` rule is the review's fix as written (`m7_admission.py:63-69`):
   at a copy's admission with number `r` the root commits the offer `r+1` but keeps `r` as the
   record, so an original still presenting `r` is admitted after the copy ran and died (S5). The
   claim holds; the root cannot learn that the node saved the offer without a third message. The
   third step locks nothing out: every crash point leaves the node holding the record (a fresh offer
   follows) or the pending offer (completed), and a lost answer at either step falls back to one of
   the two (S2 at four points, S6 at three, S7 at six).
2. **F2 across leader changes.** The admitted boot is a committed field, so every new leader has it.
   The rule holds while a running original reaches a new leader within the 2 s counted from its
   election. S4 does not test that bound: `Root.silent` changes only in `died()`
   (`m7_admission.py:47-52`), so its 0% holds by construction, and `TimedRoot` has no leader change
   (F19).
3. **Revision 2's retry.** A copy beside a live original is refused at every try, since the original
   is heard every 500 ms (`src/cluster/rpc_methods.h:131`), and exits at 2.5 s plus its start-up
   time: inside 3 s if it reaches its first hello within 0.5 s (S8 counts from the first hello,
   F27). The retry lets a copy in only where the original went 2 s unheard, Limit 2's case; a
   supervisor restarting an exiting copy reached the same exposure before. A crash restart outlasts
   the window as long as the 2.5 s run from the first `duplicate` and cover any hello before
   admission, including a start that straddles a root leader change (F27).
4. **Crash points with the run command saving.** With `join` gone, the process that saves the key,
   the identity and the offer is the node itself. S7 kills an empty directory's first start at all
   six points (key, join commit, id save, offer, save, confirm) and restarts it at once: one id, the
   use kept. S6 kills a restart at the three admission points with restarts 0 to 6 s later. The one
   new state is a directory holding a saved key and no identity; the model joins it with the key,
   the table (`cluster-membership.md:33-40`) has no row for it (F20). A carry killed after the
   identity is saved restarts as a member of the token's cluster and runs, so the start resumes the
   carry's markers (`:82`); that is now one command and needs no change.
5. **Cold start and Limit 2.** Without a leader no offer can be committed, so a voter's check is
   equality against its own copy (`:154-157`). Limit 2 names a restored root replica, not a copied
   one; with one lane per node at each peer, a voter and its copy started cold can be accepted at
   different peers and vote for two candidates in one term (F22). The false `stale` is F17.
6. **F3.** `known` (`m9_survivor.py:111-113`) cannot lose an entry another survivor heard committed:
   every committed prefix a survivor heard lies inside the highest one, which every candidate holds,
   and the survivor that heard it is a candidate. Consensus persists the commit cursor
   (`consensus:ARCHITECTURE.md:89`), so the heard index survives the survivors' own restarts.
7. **The folds.** F4: the key is saved before the first request (`:101`) and addition 2 takes it; S7
   covers it. F5: one root command with its criterion (`:129-130`). F6: the pinned-key query
   answers `retired` or `unknown` (`:161-162`) and the id skip is bounded (`:203`). F8: its own pool,
   refusing rather than evicting (`:99-101`). F9: Limit 4 reworded. F7, F10, F11 and F13 to F16 are
   folded as listed; F12 is closed by D157, one run command on every node.
8. **The join cap.** 64 at once and 8 per source suit a StatefulSet, whose pods dial from their own
   addresses, and joiners behind one NAT: a join holds a slot for one root commit, so 10 a second per
   source admits 1,000 nodes behind one address in 100 s. Renewals share the exchange and average
   far below one a second per 100,000 nodes at a one-year life; a CA replacement spreads over every
   member's pool. The cap names no slot deadline (F25).
9. **D157's local routes.** A peer-address check for loopback also passes every request a local
   reverse proxy or a service-mesh sidecar forwards (F28).

### Status of the first review's findings

| ID | Status |
|---|---|
| F1 | closed: the three-step rule refuses S1, S3 and S5 and locks out at no crash point; F17 is a new edge of the same rule |
| F2 | closed as a rule (addition 7, the committed boot), with the restart retry of revision 2; F19 is its window at a leader change |
| F3 | closed; F23 names what addition 6 must expose for it |
| F4 | closed, and modeled (S7) |
| F5 | closed |
| F6 | closed; F17 covers the remedy `unknown` prints |
| F7, F9 | closed |
| F8 | closed; F25 |
| F10 | closed by D157's local-only routes (Limit 6); F28 |
| F11 | closed, pending Detent's lead |
| F12 | closed by D157 |
| F13 | closed (the carry closes each group; the condition is the owner's, D154) |
| F14, F15, F16 | closed |
| F18 | not raised: revision 1's exit on the first `duplicate` read a supervisor's immediate restart as a copy (S6 without the retry); revision 2's retry closed it before this re-check was written |

### New findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F17 | yes | defect | `stale` is any number but the record or the pending offer, so it is wrong wherever the record lags the node. At a cold start a root voter checks against its own copy, which may lack the voter's last refresh, and the legitimate voter reads `stale`. After a root recovery that lost a node's last refresh or restart, the node presents a number above the record and reads `stale`; one whose join was lost reads `unknown`. Both remedies say to empty the directory (`:159-161`), on a node holding the newest state, possibly a survivor an index's own `recover` needs. Revision 0 accepted any number above the record (`m7_admission.py:64`, rule `design`); the equality rule is new, and no model covers a cold start or a root recovery. | `cluster-membership.md:147-152`, `:154-157`, `:159-161`, `:201-202` | `stale` only for a number below the record. At a cold start a voter answers a number above its copy's record by waiting for the first leader, which runs the three steps. After a root recovery, a node presenting a number above its record, or a pinned key the root lost, is re-admitted by the three steps under its id (the id counter already skips past it). Criteria: a cold start with one voter's copy behind admits every voter; after a recovery that lost one node's refresh and another's join, both rejoin with their data |
| F19 | no | gap | The new leader's 2 s must cover a running original's rediscovery after the old leader dies: its 500 ms keepalive failing, a leader lookup and a TLS dial, while a copy retries at full speed. Nothing shows that bound. | `:140-143`; `m7_admission.py:47-52`, `:283-303` | Derive the window from the keepalive and redial bounds. The criterion's 20 leader changes include killing the leader's host |
| F20 | no | nit | A directory with a saved key and no identity has no row; the model joins it with the key (`m7_admission.py:314-323`). Without a token it would found and strand the root's record if the join had committed. | `:33-40`, `:101` | A row: a saved key, no identity: joins with the key given a token, refused naming the token without one |
| F21 | no | gap | After `leave` deletes the directory (`:124`), nothing says whether the process exits. If it does, the supervisor restarts it with the token, which D157 puts on every start, and it joins as a new node before scale-down lowers `replicas`; then scale-down kills it and it stays `down` until `remove`. | `:124`, `:216-220` | The owner decides. Options: (a) the deletion keeps a `retired` marker, and a start on it exits `retired` until the operator clears it, or with `--token` after a new `invite`; (b) the retired process stays up, serving nothing, until stopped; (c) keep it and say a deployment stops the node before `leave` returns |
| F22 | no | nit | Limit 2 names a root replica restored while the root has no leader; a copied one started cold does the same. The cost is more than a double vote: the root's commits can roll back (ids reissued, retired pins back, token uses reset). | `:154-157`, `:258-260` | "a root replica copied or restored and started while the root has no leader", naming the root's rollback |
| F23 | no | nit | The `known` filter needs each survivor's term at the highest heard commit index, and the plan needs the entries; addition 6 exposes only last term, last index and commit index. | `:238-239`; `m9_survivor.py:113` | Addition 6 also reads a replica's term at an index and its entries past a point |
| F24 | no | nit | "An acceptor behind the directory version that admitted a boot fetches" needs the acceptor to know it is behind; addition 7 does not say the hello carries that version. | `:145`, `:240-241` | The hello carries the admitting version, or an acceptor fetches on any boot it does not know |
| F25 | no | nit | The join pool names no slot deadline, so 64 idle certificate-less sockets refuse every join for as long as they stay open. | `:99-101` | A deadline per slot for the handshake and the request |
| F26 | no | nit | Rows 3 and 4 overlap for a founder holding its own live token, which under D157 is every start of Kubernetes' `xapiand-0` until a second pod joins (`:218-220`): read literally, row 4 refuses it. Row 5 omits `unknown`. | `:37-40` | Row 4: a cluster of one with a live token, given another cluster's token. Row 5 adds `unknown` |
| F27 | no | nit | "Retries a `duplicate` hello for 2.5 s" says neither when the 2.5 s start nor that it covers any hello before admission; counted from process start, a start straddling a root leader change can still exit. The 3 s criterion includes start-up time, while S8 counts from the first hello. | `:143-144`, `:168`; `m7_admission.py:327-330`, `:371-380` | 2.5 s from the first `duplicate`, at any hello before admission; the criterion counts from the copy's first hello |
| F28 | no | gap | The membership routes answer "only on loopback" (`:96`). Behind a local reverse proxy or a service-mesh sidecar that forwards inbound traffic from a loopback address, every remote client passes that check and can mint tokens, remove nodes or recover. Limit 6 names local users only. | `:95-97`, `:256-257` | The owner decides (D157). Options: (a) the routes answer only on the 0600 Unix socket (recommended); (b) a separate listener bound to 127.0.0.1 rather than a peer-address check; (c) keep it and add proxies and sidecars to Limit 6 |
| F29 | no | nit | Revision 2 dropped the comparison table's "Dead node" row, and Limit 6 sits between Limits 1 and 2. | `:245-250`, `:252-258` | Restore the row; renumber the limits |

### Not run

No model covers a cold start, a root recovery's effect on admission (F17) or a root leader change
with time (F19); those findings rest on the document's text. I did not extend the models. Scale past
100 nodes, the Kubernetes harness and M10 remain unchecked.
