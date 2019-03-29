---
title: Open / Close Index APIs
short_title: Open/Close APIs
---

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The _Open / Close Index APIs_ allow to close an index, and later on opening it.
A closed index has almost _no overhead_ on the cluster, and is blocked for
read/write operations. A closed index can be opened which will then go through
the normal recovery process.

The REST endpoint command are `CLOSE` and `OPEN`. For example:

{% capture req %}

```json
CLOSE /my_index/
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}

```json
OPEN /my_index/
```
{% endcapture %}
{% include curl.html req=req %}
