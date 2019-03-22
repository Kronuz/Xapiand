---
title: Monitoring API
---

You can retrieve information about the Xapiand server usage by using the
`:metrics` endpoint.

This returns a [Prometheus](https://prometheus.io){:target="_blank"}
compatible response with a bunch of useful metrics.

{% capture req %}

```json
GET /:metrics
```
{% endcapture %}
{% include curl.html req=req %}


<div style="min-height: 400px"></div>
