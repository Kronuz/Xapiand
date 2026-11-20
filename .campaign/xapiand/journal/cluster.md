# cluster: journal

State: waiting on jobs 261120-104100-17872a 261120-104101-6fec3b 261120-104101-0779b2 261120-104101-65db6c (suite), 261120-104107-5a4591 (A5 tests), 261120-104118-187dc4 then 261120-104125-6ca616 261120-104127-4d45de 261120-104128-564b3a (fuzz); 261120-095830-2935c9 261120-095833-f98de7 (A5 TSan, ASan at Xapiand:37f0c5a5f)
Quest: .campaign/xapiand/quests/cluster.md (the lead's; never edit it)

## Doing

B1 answered (lead: `CMakeLists.txt` and `cmake/**` are in the file set). At Xapiand:f49d0e0f7:
consensus is pinned at consensus:e1f249d8d (main, the RPC lane), task 18eb516 to 95798a02f, journal
5bbeedc to 9e11713ed and completion e49f288 to d8b538c67 (the commits consensus pins; consensus's
reactor pin ab69cf2 is older than Xapiand's 886131b, kept). `tests/cluster/` is no longer a
standalone project: the top-level build adds it under `BUILD_TESTS` with TLS on, and
`consensus` is made available only then. Configure and the three cluster binaries build clean
locally (macOS x86_64, scratch build; `codec` and `malformed` pass). The RPC decoders' libFuzzer
target `fuzz_rpc_methods` is under `-DFUZZ=ON` beside the other five (`fuzz_rpc_seed DIR` writes
the seed corpus). Task `rpc` (A5) otherwise as before: `src/cluster/rpc*`,
`tests/cluster/test_rpc*`, `docs/mechanisms/shard-rpc.md`.

Jobs at Xapiand:f49d0e0f7:
- suite (`campaign verify cluster`): build-test 261120-104100-17872a (now runs tests/cluster too),
  e2e 261120-104101-6fec3b, cluster 261120-104101-0779b2, no-walk 261120-104101-65db6c;
- A5's tests: 261120-104107-5a4591 (the 12 `cluster_rpc*` cases through the top-level build);
- fuzz, one hour as four jobs: smoke 261120-104118-187dc4 (60 s, builds `build-fuzz` with Homebrew
  clang 23 and proves the target runs), then three 30-minute jobs, `-max_total_time=1500` each
  (75 minutes in all), seeds 1, 2 and 3, each in its own corpus dir from `fuzz_rpc_seed`, `--after`
  the smoke: 261120-104125-6ca616 (seed 1), 261120-104127-4d45de (seed 2), 261120-104128-564b3a
  (seed 3). They all run on the quest's one slot, so one at a time.
- still pending from before: A5 TSan 261120-095830-2935c9 and ASan+UBSan 261120-095833-f98de7 at
  Xapiand:37f0c5a5f (the RPC sources are unchanged since; the test build moved).

## Next

1. Read the jobs above; fold in any red. If the smoke fails to build with clang 23, the fuzz jobs
   never start: fix the build, resubmit. The A5 control (a mutant that must fail) is still the one at
   37f0c5a5f (mutant/xapiand/cluster-version, Xapiand:7d7a90722): rebuild it on the new build if the
   lead wants the control at this head.
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
| A5 | at Xapiand:f49d0e0f7: tests 261120-104107-5a4591, fuzz 261120-104118-187dc4 (smoke) and 261120-104125-6ca616, 261120-104127-4d45de, 261120-104128-564b3a (seeds 1-3, 25 min each); earlier, at Xapiand:37f0c5a5f: 261120-095828-2b5e4d green (12 cases at Xapiand:37f0c5a5f); control 261120-095855-16a31d fails as required (exit 8 at the range-check mutant); TSan 261120-095830-2935c9 and ASan+UBSan 261120-095833-f98de7 pending. |
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
