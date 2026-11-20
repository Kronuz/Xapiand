# endpoint-uuid: journal

State: waiting on jobs 261119-164338-ad2484 261119-164341-1d09a8 261119-164343-ac082d 261119-164344-cd862d
Quest: .campaign/xapiand/quests/endpoint-uuid.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix, tests, coverage pin, options.md and changelog lines committed; master merged (96975b389). Jobs queued.

## Next

1. Read 261119-164338-ad2484 (A1-A3 tests at the head), 261119-164341-1d09a8 (A2 fails-without at
   refs/mutants/endpoint-uuid/m1: replace_segment without the dedupe), 261119-164343-ac082d (A3 fails-without at
   m2: no port refusal), suite 261119-164344-cd862d (A4). Fill Evidence, then ready for review.

## Blocked on

Nothing. The lead widened the set (tests/test_coverage_core_endpoint.cc, options.md, changelog.md): the
coverage pin now expects the refusal (CHECK_THROW ClientError).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending; CI needs a push, which the lead holds |

Lead's message (file set widened): done in 257a2417a, coverage line flipped, options.md and changelog lines added.

## Result

src/endpoint.cc: a normalized path normalizes to itself (partitioned UUIDs are not partitioned again), a node URI
with a port is refused, a ':' in a file path stays in the name. Docs: options.md (Identifiers), changelog
Unreleased/Fixed. Tests: tests/test_endpoint.cc, tests/test_endpoint_uuid.cc, tests/test_coverage_core_endpoint.cc.

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
- tests/test_endpoint_uuid.cc copies a small node bootstrap (RunningNode, HTTP call) as test_node.cc and
  test_loop_yield.cc do; a shared tests/node_support.h would need those files, outside this set.
- Found while testing: a ':' in the first segment of a file path was cut as a port ("a:b" named "a",
  "urn:uuid:<u>" named "urn", so an HTTP index "a:b" aliased "a"). The port is now sliced only for a node URI,
  where it is refused (A3).
