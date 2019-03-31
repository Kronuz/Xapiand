---
title: Exists Index API
short_title: Exists API
---

The _Exists Index API_ is used to check if the index (indices) exists or not.

For example:

{% capture req %}

```json
HEAD /twitter/tweet/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).
