---
title: "Index Document API"
---

The _Index API_ adds or updates a typed JSON document in a specific index,
making it searchable. The following example inserts the JSON document into the
_"twitter"_ index with an ID of `1`:

```rest
PUT /twitter/tweet/1

{
    "user": "Kronuz",
    "post_date": "2019-03-22T14:35:26",
    "message": "Trying out Xapiand"
}
```

The result of the above index operation is a `204 No Content` HTTP response code
with no body. The document is searchable right away (reads see the writes of the
node). To have the operation answer the indexed document, pass the `echo` query
param (or start the node with `--echo`): the answer is then `200 OK` with

```json
{
  "user": "Kronuz",
  "post_date": "2019-03-22T14:35:26",
  "message": "Trying out Xapiand",
  "_id": 1,
  "_version": 1
}
```

(the `comments` query param adds `#docid` and `#shard`, see [Get](../get-api#comments)).
See [what a write answers](/Xapiand/quickstart#what-a-write-answers).

:::hint{.warning}
`PUT /twitter/tweet/1` is not the same as `PUT /twitter/tweet/1/`, the former
creates a document with ID `1` inside index `/twitter/tweet/` while the later
creates the index `/twitter/tweet/1/` itself.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

## Automatic Index Creation

The index operation automatically creates an index if it does not already exist.
The index operation also
creates a dynamic schema for the index if one does not already exist. By
default, new fields and objects will automatically be added to the schema
definition if needed. Check out the [Schemas](/Xapiand/reference-guide/schemas)
section for more information on schema definitions, and the
[indexes create API](/Xapiand/reference-guide/indexes/create-api)
for information about updating document schema manually.

## Automatic ID Generation

The index operation can be executed without specifying the ID. In such a case,
an ID will be generated automatically. Here is an example (note the `POST` used
instead of `PUT`):

```rest
POST /twitter/tweet/

{
    "user": "Yosef",
    "post_date": "2019-03-22T14:46:10",
    "message": "Also trying out Xapiand!"
}
```

The result of the above index operation is a `201 Created` HTTP response code
with the new document's URL in the `Location` header (with `echo`, the document
too, with its `_id` and `_version`). The ID is minted from the type of the
index's `_id` field, which the first document sets:

- On a new index, a UUID (version 6 by default, `--legacy-ids` mints version 1).
- Where the IDs are integers, as in the examples here, an integer.
- Where the IDs are `keyword` or `text`, a random string.

Sources: `src/server/search_views.cc:938-985`; `src/database/schema.cc:2713-2717,
2745-2870`; `src/opts.cc:235-238`.

## Scripting

We can execute script the same way scripts work in the [Update API](../update-api#scripting)
and as explained in the [Scripting](../scripting) section.

## Commit

By passing the `commit` query param to the request, you can ensure the operation
returns once the document changes have been committed to disk.

:::hint{.caution}
Try limiting the use of `commit` as it will hit performance.
:::

## Optimistic Concurrency Control

Index operations can be made conditional and only be performed if the last
current document version matches the passed `version` in the query param (or the
`_version` in the body). If a mismatch is detected, the operation will result in a
`409 Conflict` HTTP response code. See [Versioning](../versioning) for more details.

## Routing

The index operation is directed to one of the index's shards based on the hash of
its ID (FNV-1a over the ID's term; an index has 5 shards by default,
see [Create Index](/Xapiand/reference-guide/indexes/create-api)).
