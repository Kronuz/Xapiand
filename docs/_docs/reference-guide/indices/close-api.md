---
title: Open / Close Index APIs
short_title: Open/Close APIs
---

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

The _Open / Close Index APIs_ allow to close an index, and later on opening it.
A closed index has almost _no overhead_ on the running server. A closed index
can be opened which will then go through the normal opening process.

The REST endpoint command are `:close` and `:open`. For example:

{% capture req %}

```json
POST /my_index/:close
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}

```json
POST /my_index/:open
```
{% endcapture %}
{% include curl.html req=req %}
