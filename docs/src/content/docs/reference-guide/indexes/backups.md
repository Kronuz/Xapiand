---
title: "Backups"
---

Here we'll learn about how to backup your data by using Dump and Restore.
You can do this from the command line or using the online API.

---

## Command Line Dump and Restore

It's also possible to dump and restore documents from the command line.

### Dump

A dump of all documents in the `/twitter/tweet/` database can be saved by
running the following command:

```sh
xapiand --dump="twitter/tweet" --out="twitter.msgpack"
```

### Restore

To restore for the dump in the file `twitter.msgpack`:

```sh
xapiand --restore="twitter/tweet" --in="twitter.msgpack"
```

:::hint[Warning!]{.warning}
For all the above commands you _must_ additionally use the
**exact same parameters** you use while running the server.
:::

---

## Online Dump and Restore

It's also possible (for rather small databases) to dump and restore documents
over HTTP, using the online API. A dump is JSON, YAML or MessagePack; a restore
takes JSON, NDJSON or MessagePack.

### Dump

```rest
DUMP /twitter/tweet/
```

<!-- e2e:begin
---
description: Dump: an index
---

```js
pm.test("A dump is an array of documents", function () {
  pm.response.to.be.success;
  var docs = pm.response.json();
  pm.expect(docs).to.be.an("array");
  pm.expect(docs.length).to.be.above(0);
  pm.expect(docs[0]).to.have.property("_id");
});
```
e2e:end -->

### Restore

```rest
RESTORE /twitter/tweet/

[
  {
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!",
    "_id": 1
  },
  {
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?",
    "_id": 2
  }
]
```

A restore answers how many documents it read and indexed, and the `_id` of each:

```rest
RESTORE /backups/tweet/

[
  { "user": "Kronuz", "message": "Restored one", "_id": 1 },
  { "user": "Kronuz", "message": "Restored two", "_id": 2 }
]
```

<!-- e2e:begin
---
description: Restore: the answer
---

```js
pm.test("A restore answers its counts", function () {
  pm.response.to.be.success;
  var result = pm.response.json();
  pm.expect(result.prepared).to.equal(2);
  pm.expect(result.indexed).to.equal(2);
  pm.expect(result.total).to.equal(2);
  pm.expect(result.items.length).to.equal(2);
});
```
e2e:end -->

```rest
GET /backups/tweet/2
```

<!-- e2e:begin
---
description: Restore: the document is there
---

```js
pm.test("The restored document is there", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().message).to.equal("Restored two");
});
```
e2e:end -->



`DUMP` with a document ID dumps that one document (here, one of the restored ones):

```rest
DUMP /backups/tweet/1
```

<!-- e2e:begin
---
description: Dump: a document
---

```js
pm.test("A document dump is the document", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._id).to.equal(1);
});
```
e2e:end -->

### What a restore does

- The body is a JSON array, or NDJSON or MessagePack objects. A JSON array is buffered,
  so it is limited to 16 MiB (`413`). NDJSON and MessagePack are streamed when the request
  carries a `Content-Length`, whatever their size; a chunked body of those types is
  refused with `413`.
- It runs without the write-ahead log and without autocommit, and commits once when
  the body ends (what was indexed before an error stays). Documents are
  searchable when it answers.
- A restore takes a single index; a list answers `400`.
- To restore into a different schema, create the index with that schema first (see
  below). A dump written by the alpha releases restores into this release: see
  [Upgrading](/Xapiand/upgrade#data-compatibility).

Sources: `src/server/search_views.cc:1643-1750` (`restore_database_view`), `:673-682` and
`:3094-3125` (what streams), `src/database/data.h:99-102` and `:1620-1625` (the dump formats).

---

## MessagePack

Instead of using plain JSON dumps, it's more efficient to use MessagePack. Set
the `Accept` header to `application/x-msgpack` to dump in that format. A `DUMP`
does not produce NDJSON: `Accept: application/x-ndjson` answers `406`. A
`RESTORE` takes NDJSON (`Content-Type: application/x-ndjson`).

```rest
DUMP /twitter/tweet/
Accept: application/x-msgpack
```

To restore those (here into a new index, `twitter/tweets`), you should specify the
`Content-Type` header accordingly:

```rest
RESTORE /twitter/tweets/
Content-Type: application/x-msgpack

@twitter.msgpack
```

When using curl, make sure to use `--data-binary` and not simply `-d` or `--data`:

```sh
curl -X RESTORE 'localhost:8880/twitter/tweets/' \
     -H 'Content-Type: application/x-msgpack' \
     --data-binary '@twitter.msgpack'
```

---

## Restore using different schema

If you need a different schema for the dumped documents, before restoring, it's
also possible to set a new schema for the new index and then restore the
documents to that index:

Create a new schema for the new database; in this example we'll create a
[foreign schema](/Xapiand/reference-guide/schemas/foreign-schemas)
to reindex the dumped documents:

```rest
PUT /twitter/tweets/

{
  "_schema": {
    "_type": "foreign/object",
    "_foreign": ".schemas/00000000-0000-1000-8000-010000000000",
    "_meta": {
      "description": "Twitter Schema"
    },
    "_id": {
      "_type": "integer",
    },
    "user": {
      "_type": "keyword"
    },
    "postDate": {
      "_type": "datetime"
    },
    "message": {
      "_type": "text"
    }
  }
}
```

Restore the index documents to the new index:

```rest
RESTORE /twitter/tweets/

[
  {
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!",
    "_id": 1
  },
  {
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?",
    "_id": 2
  }
]
```
