---
title: Delete Index API
short_title: Delete API
---

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The _Delete Index API_ allows to delete an existing index.

{% capture req %}

```json
DELETE /twitter/tweet/
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
`DELETE /twitter/tweet/` is not the same as `DELETE /twitter/tweet`, the former will delete the
whole index `/twitter/tweet/` while the later will delete document `twitter/tweet` inside
index `/`.
[Trailing slashes are important]({{ '/docs/reference-guide/api#trailing-slashes-are-important' | relative_url }}).
