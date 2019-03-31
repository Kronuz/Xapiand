---
title: Document Information API
short_title: Doc Info API
---

You can retrieve information about a given document using the `INFO` method
and passing the document ID as part of the `GET` query:

{% capture req %}

```json
INFO /twitter/tweet/1
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).

The response will include a set of valuable information about the required
document:

* `docid`      - Internal document ID.
* `data`       - Content types stored in the document data.
* `terms`      - Object containing all the terms indexed by the document.
* `values`     - Object containing all values stored by the document.
