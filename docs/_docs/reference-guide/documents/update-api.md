---
title: Update API
---

The _Update API_ allows to delete a document based on the existent content of
a document. The operation gets the current document from the index, merges the
new data with the data in the current document, optionally it can also run a
script, and indexes back the result. It internally and automatically uses
versioning to make sure this operation is atomic.

If the document does not already exist, the body of the request will be added
as a new document, the exact same way as the [Index API](../index-api) does it,
just just the little extra overhead of trying to retrieve the current document
first.

For example, let’s index a simple doc:

{% capture req %}

```json
PUT /test/1

{
  "counter" : 1,
  "tags" : ["red"]
}
```
{% endcapture %}
{% include curl.html req=req %}


## Merging

The following update adds a new field to the existing document by merging the
passed fields (simple recursive merge, inner merging of objects, replacing core
"keys/values"and arrays):

{% capture req %}

```json
UPDATE /test/1

{
  "name" : "new_name"
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
To fully replace the existing document, the [Index API](../index-api) should be
used instead.


## Scripting

Now, we can execute a script that would increment the counter:

{% capture req %}

```json
UPDATE /test/1

{
  "_script": "_doc.counter = _old_doc.counter + 1"
}
```
{% endcapture %}
{% include curl.html req=req %}

We can add a tag to the list of tags (if the tag exists, it still gets added, since this is a list):

{% capture req %}

```json
UPDATE /test/1

{
  "_script": {
    "_body": "_doc.tags.append(tag)",
    "_params": {
      "tag": "blue"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

We can remove a tag from the list of tags:

{% capture req %}

```json
UPDATE /test/1

{
  "_script": {
    "_body": "_doc.tags.erase(tag)",
    "_params": {
      "tag": "blue"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

We can also programmatically add a new field to the document:

{% capture req %}

```json
UPDATE /test/1

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
{% endcapture %}
{% include curl.html req=req %}

Or remove a field from the document:

{% capture req %}

```json
UPDATE /test/1

{
  "_script": {
    "_body": "_doc.erase(field)",
    "_params": {
      "field": "new_field",
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

See the [Scripting](../scripting) section for more details about scripts
and the scripting language.


## Commit

By passing `commit` query param to the request, you can ensure the operation
returns once the document changes have been committed to the primary shard.

{: .note .caution }
Try limiting the use of `commit` as it will hit performance.


## Optimistic Concurrency Control

Delete operations can be made optional and only be performed if the last
current document version matches the passed `version` in the query param. If a
mismatch is detected, the operation will result in a `409 Conflict` HTTP response
code. See [Versioning](../versioning) for more details.
