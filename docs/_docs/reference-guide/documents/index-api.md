---
title: Index API
---

The _Index API_ adds or updates a typed JSON document in a specific index,
making it searchable. The following example inserts the JSON document into the
_"twitter"_ index with an ID of `1`:

{% capture req %}

```json
PUT /twitter/1

{
    "user" : "Kronuz",
    "post_date" : "2019-03-22T14:35:26",
    "message" : "Trying out Xapiand"
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
  "_id": 1,
  "_version": 1,
  "#docid": 1,
  "#shard": 1
}
```


## Automatic Index Creation

The index operation automatically creates an index if it does not already exist,
and applies any index templates that are configured. The index operation also
creates a dynamic schema for the index if one does not already exist. By
default, new fields and objects will automatically be added to the schema
definition if needed. Check out the [Schemas]({{ '/docs/reference-guide/schemas' }})
section for more information on schema definitions, and the [put schema API]()
for information about updating document schema manually.


## Automatic ID Generation

The index operation can be executed without specifying the ID. In such a case,
an ID will be generated automatically. Here is an example (note the `POST` used
instead of `PUT`):

{% capture req %}

```json
POST /twitter/

{
    "user" : "Yosef",
    "post_date" : "2019-03-22T14:46:10",
    "message" : "Also trying out Xapiand!"
}
```
{% endcapture %}
{% include curl.html req=req %}

The result of the above index operation is:

```json
{
  "user": "Yosef",
  "post_date": "2019-03-22T14:46:10",
  "message": "Also trying out Xapiand!",
  "_id": 2,
  "_version": 1,
  "#docid": 2,
  "#shard": 2
}
```

## Scripting

We can execute script the same way scripts work in the [Update API](../update-api#scripting)
and as explained in the [Scripting](../scripting) section.


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


## Replication

The index operation is directed to the primary shard based on the hash of its ID
and performed on the actual node containing this shard. After the primary shard
completes the operation, if needed, the update is synchronized to all applicable
replicas.
