---
title: Exists Document API
short_title: Exists API
---

The _Exists Document API_ allows to check for the existence of a document using
`HEAD` method.

For example:

{% capture req %}

```json
HEAD /twitter/tweet/1
```
{% endcapture %}
{% include curl.html req=req %}

The result of the above operation is a `200 OK` HTTP response code with no body.

{: .note .warning }
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).
