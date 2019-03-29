---
title: Get Index API
short_title: Get API
---

The _Get Index API_ allows to retrieve information about one or more indexes.

{% capture req %}

```json
GET /twitter/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).
