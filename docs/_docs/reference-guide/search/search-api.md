---
title: Search API
---

You can search for documents by using the `SEARCH` method:

{% capture req %}

```json
SEARCH /bank/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .tip }
It is also possible to use [HTTP Method Mappings]({{ '/docs/reference-guide/api#http-method-mapping' | relative_url }}).
