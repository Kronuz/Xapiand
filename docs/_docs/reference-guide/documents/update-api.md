---
title: Update Document API
short_title: Update API
---

The _Update API_ allows to delete a document based on the existent content of
a document. The operation gets the current document from the index, merges the
new data with the data in the current document, optionally it can also run a
script, and indexes back the result. It internally and automatically uses
versioning to make sure this operation is atomic.

If the document does not already exist, Xapiand will return 404 by default.
This can be changed by passing `upsert` as a query param, in this event, the
body of the request will be added as a new document, the exact same way as the
[Index API](../index-api) does it, just just the little extra overhead of trying
to retrieve the current document first.

For example, let's index a simple doc:

{% capture req %}

```json
PUT /twitter/tweet/1

{
  "user": "Kronuz",
  "post_date": "2019-03-22T14:35:26",
  "message": "Trying out Xapiand",
  "likes": 0,
  "hashtags": ["#xapiand"]
}
```
{% endcapture %}
{% include curl.html req=req %}

The result of the above index operation is:

```json
{
  "user": "Kronuz",
  "post_date": "2019-03-22T14:35:26",
  "message": "Trying out Xapiand",
  "likes": 0,
  "hashtags": [
    "#xapiand"
  ],
  "_id": 1,
  "_version": 1,
  "#docid": 1,
  "#shard": 1
}
```

{: .note .warning }
`PUT /twitter/tweet/1` is not the same as `PUT /twitter/tweet/1/`, the former
creates a document with ID `1` inside index `/twitter/tweet/` while the later
creates the index `/twitter/tweet/1/` itself.
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).


## Merging

The following update adds a new field to the existing document by merging the
passed fields (simple recursive merge, inner merging of objects, replacing core
"keys/values"and arrays):

{% capture req %}

```json
UPDATE /twitter/tweet/1

{
  "title": "Xapiand Rocks!"
}
```
{% endcapture %}
{% include curl.html req=req %}

As expected, the result of the merge operation is:

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
  "_version": 12,
  "#docid": 1,
  "#shard": 1
}
```

{: .note .caution }
To fully replace the existing document, the [Index API](../index-api) should be
used instead.


## Scripting

Now, we can execute a script that would increment the number of likes:

{% capture req %}

```json
UPDATE /twitter/tweet/1

{
  "_script": "_doc.likes = _old_doc.likes + 1"
}
```
{% endcapture %}
{% include curl.html req=req %}

We can add a tag to the list of hashtags (if the tag exists, it still gets added, since this is a list):

{% capture req %}

```json
UPDATE /twitter/tweet/1

{
  "_script": {
    "_body": "_doc.hashtags.append(tag)",
    "_params": {
      "tag": "#trying"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

We can remove a tag from the list of hashtags:

{% capture req %}

```json
UPDATE /twitter/tweet/1

{
  "_script": {
    "_body": "_doc.hashtags.erase(tag)",
    "_params": {
      "tag": "#trying"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

We can also programmatically add a new field to the document:

{% capture req %}

```json
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
{% endcapture %}
{% include curl.html req=req %}

Or remove a field from the document:

{% capture req %}

```json
UPDATE /twitter/tweet/1

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
