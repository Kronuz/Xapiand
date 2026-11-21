# cluster: journal

State: waiting on jobs 261121-085756-35e5b9 (A5 under TSan x3, own build dir; e6cd96 never built: build-cluster-tsan was configured from tests/cluster) 261121-054412-d9be07 (A5 plain) at Xapiand:4fa179c7c; CI on the push
Quest: .campaign/xapiand/quests/cluster.md (the lead's; never edit it)

## Doing

2026-11-21: read the 2026-11-20 jobs and CI at Xapiand:44c2ba5e4; fixed this quest's one red,
merged master (clean, Xapiand:4fa179c7c), pushed. Membership design is at revision 3
(`docs/design/cluster-membership.md`), waiting on the lead's design round; nothing of it is built.

Failures and whose they are:

| Failure | Cause | Owner |
|---|---|---|
| TSan `cluster_rpc_streams`: 261120-095830-2935c9 (exit 8), CI "Unit tests under TSan" | `test_streams`' forward_write handler ran `CHECK_EQ` on the loop thread, racing `testh::checks` | this quest; fixed at Xapiand:a2470c24a (the handler records an atomic, the main thread checks) |
| build-test 261120-104100-17872a (exit 1; ctest 89/89 green), CI "Blocking calls on reactor paths", arm64 and FreeBSD rows `t1_strict_owed` and `gate_tools` | `async_check --strict`: `async_allowlist.tsv:37` `src/engine/strata/shard.cc` 6 hits for 5 allowed, `pool.cc` row stale; files this branch never touched | master at the merge base Xapiand:b9de2ed42; fixed on master by Xapiand:bf593a759 and Xapiand:7290c10d8, in this branch by the merge |
| cluster 261120-104101-0779b2 (exit 2) | `harness/run_all.sh cluster`: "unknown layer: cluster"; run_all.sh has no `cluster` layer (the tier-1 entry is `harness/cluster/run.sh check`) and `run_all.sh` is outside this file set | the lead's check definition (or `cluster-harness`): point the check at `harness/cluster/run.sh check --nodes 3` / `--nodes 5`, or add the layer |
| no-walk 261120-104101-65db6c (exit 1) | `expand(Endpoint::cwd` is still in `src`: removing it is task `search` (A3), not built | expected red until `search` |

Passed and standing: e2e 261120-104101-6fec3b; A5 tests 261120-104107-5a4591; fuzz smoke
261120-104118-187dc4 and seeds 1-3 261120-104125-6ca616, 261120-104127-4d45de,
261120-104128-564b3a (75 min, no crash); ASan+UBSan 261120-095833-f98de7.

## Next

1. Read 261121-054409-e6cd96 and 261121-054412-d9be07 and CI at the pushed head; the A5 control
   (mutant/xapiand/cluster-version) is still at Xapiand:37f0c5a5f.
2. `membership` once B2 and B3 are answered and the design round passes: node identity, `--join`,
   keepalives, `suspect`/`down` over the `keepalive` method, the directory behind an interface the root fills (placement).
3. `shard-map`, `search`, `migration`, `api`: after `group-core`, `journal-replaces-wal` and
   `placement` publish their contracts (none is built yet; see Dependencies).
4. Multicast wiring (`--discovery-*` onto consensus `Seeds`) last, after `multicast-seeds` lands.

## Dependencies at 2026-11-20

| Contract | State | What it gates here |
|---|---|---|
| `consensus:rpc-lane` | landed, consensus:e1f249d | `rpc` (buildable now) |
| `consensus:seed-discovery` | `multicast-seeds` active (consensus `quest/xapiand/multicast-seeds`: `Seeds::on_dial`, `joined`, `failed`) | multicast wiring, last |
| `strata:group`, `strata:group-install` | `group-core` active, `group-heal` planned | `shard-map`, `migration` |
| `strata:placement-records`, `placement-repair` | `placement` planned | directory, `shard-map`, patterns (`search`), `api` drain and replicas |
| `xapiand:command` | `journal-replaces-wal` planned | `shard-map` (forward_write's body), `migration` |
| `prospect:supplied-stats` | landed, Xapiand:2202030d2 | `search` |

## Blocked on

- B1 answered by the lead: `CMakeLists.txt` and `cmake/**` are in the file set; done at
  Xapiand:f49d0e0f7 (pins and `add_subdirectory(tests/cluster)`). `src/cluster/*` is still not in
  `XAPIAND_SRC_LIST`: the server does not call the RPC yet, so the tests build it themselves; the
  first caller (`membership`) adds it, with consensus linked into the server.
- B2 (lead, consensus): `HostTransport`, `Links` and `NodePins` take every node's address and pin at
  construction (consensus:e1f249d `host_transport.h:270`, `links.h:50`, `node_pins.h:14`); nothing
  adds or forgets a node later, and a dial needs the peer's node id. `--join` (A4: three nodes
  from one seed) and multicast seeds (`Seeds::on_dial` hands an address, not a node) need both.
  Recommendation: a consensus quest `node-admission`: `HostTransport::admit(node, address, pin)`
  and `forget_node(node)` on every loop, and a seed dial by address that returns the authenticated
  node. Detent's `Seeds` wiring needs the same, so it belongs in consensus, not here.
- B3 (owner via lead): how a joiner's pin reaches the cluster. Q5 (b) is the pki offline CA; A4
  refuses "one whose certificate is not pinned". Options:
  (a) the root's directory is the pin list: an operator issues the node's certificate with
      `xapiand pki` and adds its pin (`POST /:nodes`) before it joins;
  (b) the cluster CA is the trust anchor: a CA-signed certificate whose SPIFFE id names this
      cluster is admitted at first join and its pin recorded in the directory; from then on that
      name is pinned, so a re-keyed or second certificate for it is refused until an operator
      removes the node;
  (c) a static pin file every node reads (`--peer-pins`), as consensus's tests do.
  Recommendation: (b): zero-touch joins for the demo (D141), the CA check Detent makes, and pinning
  from first join on. Refusals named `cluster` (another cluster's SPIFFE id or hello) and
  `unpinned` (no chain to the cluster CA, or a name pinned to another key).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | none yet |
| A2 | none yet |
| A3 | none yet |
| A4 | none yet |
| A5 | at Xapiand:f49d0e0f7: tests 261120-104107-5a4591, fuzz 261120-104118-187dc4 (smoke) and 261120-104125-6ca616, 261120-104127-4d45de, 261120-104128-564b3a (seeds 1-3, 25 min each, no crash); at Xapiand:37f0c5a5f: ASan+UBSan 261120-095833-f98de7 green, TSan 261120-095830-2935c9 red (test race, fixed at Xapiand:a2470c24a); control 261120-095855-16a31d fails as required; at Xapiand:4fa179c7c: TSan x3 261121-054409-e6cd96, plain 261121-054412-d9be07 pending. |
| A6 | none yet |

## Tasks for other agents

Parallel writers pay only once the strata contracts exist. Once `group-core` and
`journal-replaces-wal` land, `shard-map` and then `search` (after `shard-map`'s contract) are the
two worth their own agents; `migration` and `api` follow. `membership` waits on B2 and B3.

## Result

Not yet.

## Rulings I made

- R11: the cluster-check and no-walk reds are not this quest's code to fix now (the first is the
  check's command, outside the file set; the second is task `search`); the master merge is a plain
  `git merge master`, not `campaign land --merge-main`, which may land.

- R3: application failures ride the lane's `Error` frame (code 256 + Status, text), never a
  response body, so a stream that fails midway and one refused up front look the same to the
  caller; `not_leader`'s hint and `version_refused`'s range are decimal text.
- R4: an envelope is at most 16 MiB (Xapiand's largest buffered body, `search_service.h`);
  bodies and blobs are payloads, streamed, so the bound never caps a document.
- R5: `forward_write` and `get` relay the HTTP request itself (`Relayed`/`Answer`), so the
  serving node runs the handler a local request runs and `hops` carries D8's bound.
- R6: payload bytes that arrive before a handler sets its sink stay unconsumed, holding the
  caller's credit instead of extra memory.

- R1: the shard RPC carries its own version in every call's header (u16, the range `[1, 1]` now)
  instead of riding the lane's hello version (`Link::rpc`), which selects the lane's frame magic
  and is consensus's. A call outside the server's range is answered `version_refused` with the
  server's range, which the client names `PeerVersionRefused`.
- R2: method ids are `0x58530000 + n` ("XS"), so other users of the same host transport
  (consensus's own methods, Detent's fetch) never collide with Xapiand's.
- R7: completion is pinned in Xapiand too (d8b538c67, newer than strata's e49f288): journal 9e11713ed
  needs it, and its `completion::task` needs `COMPLETION_TASK` on before strata makes completion
  available, so the root sets it. journal is declared with no Xapiand include site, so
  `harness/pins_check.py` A1 now also lists journal (varint was already listed); the lead may
  prefer journal declared by strata at its next pin and the declaration dropped then.
- R8: `tests/cluster/CMakeLists.txt` drops the root's `MSGPACK_EXCEPTION_HEADER` and
  `RAPIDJSON_PARSE_DEFAULT_FLAGS` definitions in its directory, so the cluster code builds against
  `msgpack::msgpack` exactly as the library ships it (the build that was green standalone); linking
  `XAPIAND_OBJ` instead would pull the whole server into a decoder fuzz target.
- R9: the fuzz target adds `-fno-sanitize-recover=all` to the other targets' flags, so a UBSan
  report stops the run and leaves a crash file. Its oracle: a decoder returns a value whose
  re-encoding decodes to the same value, or throws `Malformed` (MsgPack-holding messages are
  re-decoded only: NaN never equals itself).
- R10: the hour is three 25-minute runs (75 minutes, seeds 1 to 3) behind a 60-second smoke: a
  light fuzz job is capped at 30 minutes including the run.
