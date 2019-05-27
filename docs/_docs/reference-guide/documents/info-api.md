---
title: Document Information API
short_title: Doc Info API
---

You can retrieve information about a given document ID using the `INFO` method:

{% capture req %}

```json
INFO /twitter/tweet/1
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
`INFO /twitter/tweet/1` is not the same as `INFO /twitter/tweet/1/`, the
former will retrieve information about document `1` inside index `/twitter/tweet/`
and the later will retrieve information about thewhole index `/twitter/tweet/1/`.
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).

The response will include a set of valuable information about the required
document:

* `docid`      - Internal document ID.
* `data`       - Content types stored in the document data.
* `terms`      - Object containing a tree with all the terms indexed by the document.
* `values`     - Object containing all values stored by the document.
