---
title: Information API
short_title: Info API
---

You can retrieve information about a given document using the `:info` endpoint
and passing the document ID as part of the `GET` query:

{% capture req %}

```json
GET /bank/:info/1
```
{% endcapture %}
{% include curl.html req=req %}

The response will include a set of valuable information about the required
document:

* `docid`      - Internal document ID.
* `data`       - Content types stored in the document data.
* `terms`      - Object containing all the terms indexed by the document.
* `values`     - Object containing all values stored by the document.
