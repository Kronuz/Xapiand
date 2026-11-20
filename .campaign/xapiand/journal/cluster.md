# cluster: journal

State: waiting on jobs 261120-095828-2b5e4d 261120-095830-2935c9 261120-095833-f98de7 261120-095855-16a31d
Quest: .campaign/xapiand/quests/cluster.md (the lead's; never edit it)

## Doing

Task `rpc` (A5) built: `src/cluster/rpc.{h,cc}` (call layer over consensus:e1f249d's lane),
`src/cluster/rpc_methods.{h,cc}` (the seven methods' MessagePack envelopes),
`tests/cluster/{test_rpc,test_rpc_methods}.cc`, `docs/mechanisms/shard-rpc.md`. Built and tested
through `tests/cluster/CMakeLists.txt`, standalone inside the file set (B1): 12 ctest cases green
locally (macOS arm64). Jobs at Xapiand:37f0c5a5f: plain 261120-095828-2b5e4d, TSan
261120-095830-2935c9, ASan+UBSan 261120-095833-f98de7; control 261120-095855-16a31d at
`mutant/xapiand/cluster-version` (Xapiand:7d7a90722, the server skips its range check:
`cluster_rpc_versions` must fail).

## Next

1. Read the four A5 jobs; fold in any red. Fuzz target for the envelope decoders (A5's decoders;
   the random-mutation case covers 20,000 inputs, not a CPU-hour) once the top-level build has FUZZ.
2. `membership` once B2 and B3 are answered: node identity, `--join`, keepalives, `suspect`/`down`
   over the `keepalive` method, the directory behind an interface the root fills (placement).
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

- B1 (lead): `CMakeLists.txt` is outside the file set, and Xapiand pins no consensus and globs
  neither `src/cluster/` nor `tests/cluster/`. Recommendation: add `CMakeLists.txt` to the file set
  for three edits only: `FetchContent_Declare(consensus ...)` with `CONSENSUS_TRANSPORT ON`,
  `src/cluster/*` in `XAPIAND_SRC_LIST`, and `add_subdirectory(tests/cluster)` under `BUILD_TESTS`.
  The pin also moves journal (Xapiand's, via strata, is journal:5bbeedc; consensus needs
  journal:9e11713, 10 commits newer) and task (Xapiand's task:18eb516 is older than consensus's
  task:95798a0): a pin bump the lead owns. Until then `rpc` builds standalone from
  `tests/cluster/CMakeLists.txt` at consensus's own pins.
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
| A5 | jobs 261120-095828-2b5e4d (suite), 261120-095830-2935c9 (TSan), 261120-095833-f98de7 (ASan+UBSan), control 261120-095855-16a31d; pending |
| A6 | none yet |

## Tasks for other agents

Parallel writers pay only once the strata contracts exist. Once `group-core` and
`journal-replaces-wal` land, `shard-map` and then `search` (after `shard-map`'s contract) are the
two worth their own agents; `migration` and `api` follow. `membership` waits on B2 and B3.

## Result

Not yet.

## Rulings I made

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
