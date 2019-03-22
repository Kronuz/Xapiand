---
title: Exists Index API
short_title: Exists API
---

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The _Exists Index API_ is used to check if the index (indices) exists or not.

For example:

{% capture req %}

```json
HEAD /twitter/?pretty
```
{% endcapture %}
{% include curl.html req=req %}
