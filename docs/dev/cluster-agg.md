> Status: CURRENT (2026-10-27) explore/cluster-agg from Xapiand:17e11d2cf; prototype, local only.

# Cluster aggregations are partial (check_at_least = 1)

## Symptom

Two nodes, 5 shards, 40 PUTs (age = k % 30 + 1; expected `_doc_count` 40, `_sum` 530, `_avg` 13.25).
`SEARCH {"_query":"*","_limit":0,"_aggs":{sum,avg}}`:

| binary | node | `_doc_count` | `_sum` | `_avg` |
|---|---|---|---|---|
| master-era (seam-write v4b) | node1 | 19 | 236 | 12.42 |
| | node2 | 26 | 314 | 12.08 |
| pre-seam (seam-write pre) | node1 / node2 | 19 / 26 | 236 / 314 | 12.42 / 12.08 |
| either, `?check_at_least=100000` | both | 40 | 530 | 13.25 |
| solo, 5 shards, any variant | | 40 | 530 | |

`:info` and the hit total (`total` 40) are right on both nodes: no document is missing, and
no replay or schema problem is involved (the values decode on both nodes).

## Mechanism

`src/database/handler.cc` (`DatabaseHandler::get_mset`, "if (aggs && check_at_least == 0)"):
aggregations default `check_at_least` to 1 ("otherwise aggregations are altogether skipped",
commit 56e1a5bd9, 2019). The spy only sees documents the matcher checks. Shards in this process are
scanned fully, but a remote shard's matcher stops after its first checked document(s), so each
node's aggregate is its local shards plus a sliver of the remote ones. The two nodes hold
different local shards, so their partial sums overlap and do not add up to the total
(236 + 314 = 550, not 530; the CR1 ledger's 494 + 297 = 791, not 780). The merge itself is
right (`Aggregation::merge_results`, `src/aggregations/aggregations.cc`): with
`check_at_least` large both nodes return the exact 530.

(The remote server side is `src/server/remote_protocol_views.cc`, GPL, not read.)

## Since when

Old: the default dates from 2019 and the pre-seam binary behaves identically; the Gate 0.5
T4 check only counts documents and reads. Any multi-node aggregation without an explicit
`check_at_least` has been partial. Not a regression of the seam; not the numeric range bug (no ranges used).

## Prototype

`get_mset` sets `check_at_least` to the maximum `doccount` when aggregations are given and the
request has none. `harness/cluster_check.sh` step [3b] writes 40 numeric documents and asserts
`_doc_count`, `_sum`, `_avg` on every node. Result: see below.

With the change, `harness/cluster_check.sh 2` is GREEN including [3b]: both nodes return
`_doc_count` 40, `_sum` 530, `_avg` 13.25. On the unchanged binaries the same request returns
19/236 and 26/314 (table above), which step [3b] rejects. Not done: ASan, the E2E suite, a 3-node run,
the cost of a full-scan default for selective queries (the solo path already scans fully).
