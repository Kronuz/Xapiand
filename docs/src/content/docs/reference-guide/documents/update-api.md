---
title: "Update Document API"
---

The _Update API_ allows to update a document based on the existent content of
a document. The operation gets the current document from the index, merges the
new data with the data in the current document, optionally it can also run a
script, and indexes back the result. It internally and automatically uses
versioning to make sure this operation is atomic.

If the document does not already exist, `UPDATE` answers `404 Not Found`. The
`UPSERT` method does the same as `UPDATE` and, when the document does not exist,
adds the body of the request as a new document, the exact same way as the
[Index API](../index-api) does it, just with the little extra overhead of trying
to retrieve the current document first.

For example, let's index a simple doc:

```rest
PUT /twitter/tweet/1

{
  "user": "Kronuz",
  "post_date": "2019-03-22T14:35:26",
  "message": "Trying out Xapiand",
  "likes": 0,
  "hashtags": ["#xapiand"]
}
```

The result of the above index operation is a `204 No Content` HTTP response code. As
with every write, the `echo` query param makes the operation answer the stored
document, with its `_id` and `_version`.

:::hint{.warning}
`PUT /twitter/tweet/1` is not the same as `PUT /twitter/tweet/1/`, the former
creates a document with ID `1` inside index `/twitter/tweet/` while the later
creates the index `/twitter/tweet/1/` itself.
[Trailing slashes are important](/Xapiand/reference-guide/api#trailing-slashes-are-important).
:::

## Merging

The following update adds a new field to the existing document by merging the
passed fields (simple recursive merge, inner merging of objects, replacing core
"keys/values"and arrays):

```rest
UPDATE /twitter/tweet/1

{
  "title": "Xapiand Rocks!"
}
```

The stored document is now:

```json
{
  "user": "Kronuz",
  "post_date": "2019-03-22T14:35:26",
  "message": "Trying out Xapiand",
  "likes": 0,
  "hashtags": [
    "#xapiand"
  ],
  "title": "Xapiand Rocks!",
  "_id": 1,
  "_version": 2
}
```

:::hint{.caution}
To fully replace the existing document, the [Index API](../index-api) should be
used instead.
:::

## Patching and upserting

`PATCH` applies a [JSON Patch](https://www.rfc-editor.org/rfc/rfc6902) (an array
of `add`, `remove`, `replace`, `move`, `copy` and `test` operations) to the
document instead of merging a body:

```rest
PUT /updateapi/doc/1

{
  "user": "Kronuz",
  "likes": 0
}
```

```rest
PATCH /updateapi/doc/1

[
  { "op": "replace", "path": "/likes", "value": 5 },
  { "op": "add", "path": "/title", "value": "Patched" }
]
```

```rest
GET /updateapi/doc/1
```

<!-- e2e:begin
---
description: Update: PATCH applied a JSON Patch
---

```js
pm.test("The patch was applied", function () {
  pm.response.to.be.success;
  var doc = pm.response.json();
  pm.expect(doc.likes).to.equal(5);
  pm.expect(doc.title).to.equal("Patched");
  pm.expect(doc.user).to.equal("Kronuz");
  pm.expect(doc._version).to.equal(2);
});
```
e2e:end -->

`UPDATE` of a missing document answers `404`, and `UPSERT` creates it:

```rest
UPDATE /updateapi/doc/2

{
  "user": "Jane"
}
```

<!-- e2e:begin
---
description: Update: UPDATE of a missing document
---

```js
pm.test("UPDATE of a missing document answers 404", function () {
  pm.response.to.have.status(404);
});
```
e2e:end -->

```rest
UPSERT /updateapi/doc/2

{
  "user": "Jane"
}
```

<!-- e2e:begin
---
description: Update: UPSERT creates the document
---

```js
pm.test("UPSERT creates the missing document", function () {
  pm.response.to.be.success;
  pm.expect(pm.response.json().user).to.equal("Jane");
});
```
e2e:end -->

## Scripting

Now, we can execute a script that would increment the number of likes:

```rest
UPDATE /twitter/tweet/1

{
  "_script": "_doc.likes = _old_doc.likes + 1"
}
```

We can add a tag to the list of hashtags (if the tag exists, it still gets added, since this is a list):

```rest
UPDATE /twitter/tweet/1

{
  "_script": {
    "_body": "table.insert(_doc.hashtags, tag)",
    "_params": {
      "tag": "#trying"
    }
  }
}
```

We can remove a tag from the list of hashtags:

```rest
UPDATE /twitter/tweet/1

{
  "_script": {
    "_body": "for i = #_doc.hashtags, 1, -1 do if _doc.hashtags[i] == tag then table.remove(_doc.hashtags, i) end end",
    "_params": {
      "tag": "#trying"
    }
  }
}
```

We can also programmatically add a new field to the document:

```rest
UPDATE /twitter/tweet/1

{
  "_script": {
    "_body": "_doc[field] = value",
    "_params": {
      "field": "new_field",
      "value": "value_of_new_field"
    }
  }
}
```

Or remove a field from the document:

```rest
UPDATE /twitter/tweet/1

{
  "_script": {
    "_body": "_doc[field] = nil",
    "_params": {
      "field": "new_field",
    }
  }
}
```

See the [Scripting](../scripting) section for more details about scripts
and the scripting language.

## Commit

By passing the `commit` query param to the request, you can ensure the operation
returns once the document changes have been committed to disk.

:::hint{.caution}
Try limiting the use of `commit` as it will hit performance.
:::

## Optimistic Concurrency Control

Update operations can be made conditional and only be performed if the last
current document version matches the passed `version` in the query param. If a
mismatch is detected, the operation will result in a `409 Conflict` HTTP response
code. See [Versioning](../versioning) for more details.

Sources: the verbs, `src/server/search_views.cc:1000-1060` and the route table
`:575-579`; `src/database/handler.cc:603-660` (`PATCH`) and `:1087-1102`.
