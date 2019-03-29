---
title: Get Document API
short_title: Get API
---

The _Get API_ allows to get a document from the index based on its ID. The
following example gets a document from an index called _"twitter"_, with ID
valued `1`:

{% capture req %}

```json
GET /twitter/tweet/1
```
{% endcapture %}
{% include curl.html req=req %}

The result of the above get operation is a `200 OK` HTTP response code with the
following body:

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

The above result includes the `_id` and `_version` of the document we wish to
retrieve, aditionally to the actual body of the document.

If the document is not found, it will return a `404 Not Found` HTTP response code.

{: .note .warning }
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).


## Volatile

By passing `volatile` query param to the request, you can ensure the operation
will return the latest committed document from the primary shard.

{: .note .caution }
Try limiting the use of `volatile` as it will hit performance.
