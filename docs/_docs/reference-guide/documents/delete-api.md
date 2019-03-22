---
title: Delete API
---

The _Delete API_ allows to delete a document from a specific index based on its
ID. The following example deletes the document from an index called _"twitter"_
with ID `1`:

{% capture req %}

```json
DELETE /twitter/1?pretty
```
{% endcapture %}
{% include curl.html req=req %}

The result of the above get operation is a `204 No Content` HTTP response code
with no body.


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
