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

<!-- e2e:begin
---
description: Index: a document by its ID
---

```js
pm.test("The write answers the document", function () {
  pm.response.to.be.success;
  var doc = pm.response.json();
  pm.expect(doc._id).to.equal(1);
  pm.expect(doc.message).to.equal("Trying out Xapiand");
  pm.expect(doc._version).to.be.a("number");
});
```
e2e:end -->

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

<!-- e2e:begin
---
description: Index: a new ID
---

```js
pm.test("POST with echo answers 200 with a new integer ID", function () {
  pm.response.to.have.status(200);
  pm.response.to.have.header("Location");
  var doc = pm.response.json();
  pm.expect(doc._id).to.be.a("number");
  pm.expect(doc._id).to.not.equal(1);
  pm.expect(doc._version).to.equal(1);
});
```
e2e:end -->

The result of the above index operation is a `201 Created` HTTP response code
with the new document's URL in the `Location` header; with `echo` it is `200 OK`
with the same header and the document, with its `_id` and `_version`. The ID is minted from the type of the
index's `_id` field, which the first document sets:

- On a new index, a UUID (version 6 by default, `--legacy-ids` mints version 1).
- Where the IDs are integers, as in the examples here, an integer.
- Where the IDs are `keyword` or `text`, a random string.

Sources: `src/server/search_views.cc:938-985`; `src/database/schema.cc:2713-2717,
2745-2870`; `src/opts.cc:235-238`.

## Document IDs

The ID in the path takes the type of the index's `_id` field. Where the IDs are
integers, any integer is an ID, `0` and negative numbers included, and the
document answers it as a number. Where the IDs are keywords, `0` and `00` are
two IDs, each answered as a string.

```rest
PUT /idsapi/numbers/7

{
  "n": "seven"
}
```

<!-- e2e:begin
---
description: Index: the first ID sets the ID type
---

```js
pm.test("An integer ID makes the IDs integers", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._id).to.equal(7);
});
```
e2e:end -->

```rest
PUT /idsapi/numbers/0

{
  "n": "zero"
}
```

<!-- e2e:begin
---
description: Index: the integer ID 0
---

```js
pm.test("0 is an ID like any other", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._id).to.equal(0);
});
```
e2e:end -->

```rest
GET /idsapi/numbers/0
```

<!-- e2e:begin
---
description: Index: the integer ID 0 reads back
---

```js
pm.test("The document with ID 0 reads back", function () {
  pm.response.to.have.status(200);
  pm.expect(pm.response.json()._id).to.equal(0);
  pm.expect(pm.response.json().n).to.equal("zero");
});
```
e2e:end -->

```rest
PUT /idsapi/names/a

{
  "n": "a"
}
```

<!-- e2e:begin
---
description: Index: a keyword ID sets the ID type
---

```js
pm.test("A word ID makes the IDs keywords", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._id).to.equal("a");
});
```
e2e:end -->

```rest
PUT /idsapi/names/0

{
  "n": "zero"
}
```

<!-- e2e:begin
---
description: Index: the keyword ID "0"
---

```js
pm.test("\"0\" is a keyword ID", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json()._id).to.equal("0");
});
```
e2e:end -->

```rest
GET /idsapi/names/0
```

<!-- e2e:begin
---
description: Index: the keyword ID "0" reads back
---

```js
pm.test("The document with ID \"0\" reads back", function () {
  pm.response.to.have.status(200);
  pm.expect(pm.response.json()._id).to.equal("0");
  pm.expect(pm.response.json().n).to.equal("zero");
});
```
e2e:end -->

Sources: `docs/mechanisms/document-ids-and-versions.md`; `has_document_id`
(`src/database/handler.h`: any number is an ID); `tests/test_node.cc`
(`test_document_ids_are_themselves`).

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

A write is directed to one of the index's shards by its ID (an index has 5
shards by default, see [Create Index](/Xapiand/reference-guide/indexes/create-api)):

- An ID goes to one of 1,024 fixed buckets: an integer ID `k` from 1 to 2^32 - 1
  to bucket `(k - 1) mod 1024`, any other ID to the FNV-1a hash of its term
  modulo 1,024. Bucket `b` belongs to shard `b mod n` of the `n` shards.
- A document that exists is written in the shard that holds it; a new one in the
  shard its bucket belongs to.
- A `POST` to an index whose IDs are integers goes to the least used shard. It
  issues the smallest integer ID above every integer ID in the index whose bucket
  it owns, so new IDs never take an ID that was written before. With one shard
  they run 1, 2, 3, and so on, up to 2^32 - 1; past the last one, a `POST` fails.

An ID is only an ID: the document numbers each shard keeps are its own, in the
order documents arrive, and never derive from an ID.

Source: `docs/mechanisms/document-ids-and-versions.md`.
