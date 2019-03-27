---
title: Search
---

You can search for documents by using the `SEARCH` or the `GET` method:

{% capture req %}

```json
SEARCH /bank/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .tip }
It is also possible to use [HTTP Method Mapping]({{ '/docs/reference-guide/api#http-method-mapping' | relative_url }}).
