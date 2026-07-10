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
to and from JSON, NDJSON or MessagePack over HTTP, using the online API.

### Dump

```rest
DUMP /twitter/tweet/
```

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

---

## MessagePack (or NDJSON)

Instead of using plain JSON dumps, it's more efficient to use MessagePack. By
setting the `Accept` header to either `application/x-msgpack` or
`application/x-ndjson`, you can dump data to any of those formats:

```rest
DUMP /twitter/tweet/
Accept: application/x-msgpack
```

To restore those, you should specify the `Content-Type` header accordingly:

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
