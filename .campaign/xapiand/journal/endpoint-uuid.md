# endpoint-uuid: journal

State: waiting on jobs 261119-163125-f61a3a 261119-163513-04c29c 261119-163515-6c3e3e
Quest: .campaign/xapiand/quests/endpoint-uuid.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix committed (src/endpoint.cc); tests in tests/test_endpoint.cc (unit) and tests/test_endpoint_uuid.cc (node, 4
`--uuid` modes, each with two restarts; registry test under partition). First build+test job next.

## Next

1. Read jobs 261119-163125-f61a3a (tests at f3c9834de, A1-A3), 261119-163513-04c29c (A2 fails-without at
   refs/mutants/endpoint-uuid/m1: replace_segment without the dedupe), 261119-163515-6c3e3e (A3 fails-without at
   refs/mutants/endpoint-uuid/m2: no port refusal).
2. `campaign verify endpoint-uuid` (A4).

## Blocked on

Lead (file set): tests/test_coverage_core_endpoint.cc:129-130 pins the old host:port drop
(`SEE(Endpoint("xapiand://beta:8881/idx").node_name, "")`); with A3 it throws ClientError, so that test aborts and
A4's suite cannot pass. Recommended: widen the set to that file and turn the line into a CHECK_THROW of ClientError.
Also out of set, optional: a line in docs/src/content/docs/options.md (Identifiers) that a normalized UUID path
names the same index and that `xapiand://host:port/...` is refused.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- A1 finding: the rewrite is the documented v6 decode, not a bug. An index path's UUID is normalized by
  Serialize::uuid then Unserialize::uuid in the configured repr; a condensable v1 UUID's wire carries no codec
  tag, so the default v6 codec renders it as a different text (docs upgrade.md "Identifiers move to the v6
  format"; cuuid docs/CUUID_V6.md). Both texts serialize to the same bytes, so both name the one index; under
  `--legacy-ids` the v1 text stays.
- The F2 fix is in Endpoint, not in ShardEndpoint/flush: a normalized path must normalize to itself, because
  every layer rebuilds endpoints from paths (shard.cc:306/1368/1397/1898, pool.cc:895 flush,
  index_resolver_lru.cc:789, pool_support, the names a restart lists). `replace_segment` drops a partitioned
  form's directories when the path already has them just before the UUID. With Endpoint(key).path == key, F1's
  misrouting (reopen by payload path) is unreachable as well; pool.cc is not touched.
- Found while testing: a ':' in the first segment of a file path was cut as a port ("a:b" named "a",
  "urn:uuid:<u>" named "urn", so an HTTP index "a:b" aliased "a"). The port is now sliced only for a node URI,
  where it is refused (A3).
