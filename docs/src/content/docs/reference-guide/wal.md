---
title: "WAL"
---

Every shard keeps a **Write-Ahead Log** (WAL): an append-only record of the
operations applied to it (document writes and deletes, metadata changes and
commits), written alongside those changes before the shard's files are committed.
Each entry is tagged with the revision it produced, so the log is an ordered,
replayable history of the shard.

The WAL exists so that a change is never lost between the moment it is accepted
and the moment it is fully committed. A [commit](/Xapiand/reference-guide/documents/index-api#commit)
is comparatively expensive, so Xapiand does not perform one on every write;
instead the operation is appended to the WAL and the heavier commit happens in
the background (or when you ask for it explicitly). If a node stops uncleanly
before that commit, the shard recovers the next time it is opened for writing:
it replays the WAL from the last committed revision forward, re-applying exactly the
operations that had not yet made it into the index, and commits them. Recovery is
lazy: an index replays when its first write opens it, not when the node starts (only
the node's own registry indexes open at startup).

## Writing the log

WAL entries are written by a pool of asynchronous writer threads so that logging a
change stays off the request's critical path: a write is acknowledged once its log
line is queued, and the writers append and sync the lines (see
[Visibility and Durability](/Xapiand/basics#visibility-and-durability)). Two options
tune this pool:

- `--writers` sets the number of asynchronous WAL writer threads.
- `--wal-writer-cache-size` sets the writer's cache size.

The WAL is enabled by default. Certain paths manage their own durability and write with
it disabled: a restore, whether the command-line one (`xapiand --restore`) or an HTTP
[`RESTORE`](/Xapiand/reference-guide/indexes/backups), which loads its documents and commits
once at the end.

## Reading the log

The `WAL` method answers the log of an index as an array of entries, oldest first:

```rest
PUT /walapi/

{
  "_settings": {
    "number_of_shards": 1
  }
}
```

```rest
PUT /walapi/doc/1?commit

{
  "title": "logged"
}
```

```rest
WAL /walapi/
```

<!-- e2e:begin
---
description: WAL: the log of a one-shard index
---

```js
pm.test("The log is an array of entries with a revision and an op", function () {
  pm.response.to.have.status(200);
  var entries = pm.response.json();
  pm.expect(entries).to.be.an("array");
  entries.forEach(function (entry) {
    pm.expect(entry.revision).to.be.a("number");
    pm.expect(["REPLACE_DOCUMENT", "DELETE_DOCUMENT", "SET_METADATA", "COMMIT"]).to.include(entry.op);
  });
});
```
e2e:end -->

An entry is one of:

| `op` | Other members |
|---|---|
| `REPLACE_DOCUMENT` | `docid` and the `document` that was written |
| `DELETE_DOCUMENT` | `docid` |
| `SET_METADATA` | the `key` and its `data` (the index's schema is one) |
| `COMMIT` | none |

The log carries no spelling or synonym entries: nothing writes them. The query param
`raw` leaves the documents in their serialised form.

`WAL` reads one shard's log, so it answers `400` ("This operation can only be executed on
a single shard") for an index of more than one shard, and for a list of indexes
(the default is 5 shards: create the index with `number_of_shards` set to `1`, as above).

```rest
WAL /walapi-five/
```

<!-- e2e:begin
---
description: WAL: an index of several shards
---

```js
pm.test("WAL needs a single shard", function () {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("single shard");
});
```
e2e:end -->

Sources: `src/server/search_views.cc:1752-1785` (`wal_view`), `src/database/handler.cc:248-265`,
`src/database/wal.cc:324-400` (the entries); recovery, `src/database/shard.cc:453-502` and
`docs/mechanisms/wal-replay.md`.
