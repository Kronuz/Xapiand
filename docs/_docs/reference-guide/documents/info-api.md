---
title: Information API
short_title: Info API
---

You can retrieve information about a given document using the `INFO` method
and passing the document ID as part of the `GET` query:

{% capture req %}

```json
INFO /bank/1
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
`INFO /bank/1` is not the same as `INFO /bank/1/`, the former retrieves
information about a document with ID `1` inside index `/bank/` while the later
gets information about the index `/bank/1/` itself.
[Trailing slashes are important]({{ '/docs/reference-guide/api#resource-paths' | relative_url }}).

The response will include a set of valuable information about the required
document:

* `docid`      - Internal document ID.
* `data`       - Content types stored in the document data.
* `terms`      - Object containing all the terms indexed by the document.
* `values`     - Object containing all values stored by the document.
