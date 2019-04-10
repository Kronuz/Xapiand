---
title: Cluster
---

The most basic kind of information you can get is the cluster information
from the root index and the [Indices API]({{ '/docs/reference-guide/indices' | relative_url }}):

{% capture req %}

```json
GET /
```
{% endcapture %}
{% include curl.html req=req %}

The response contains:

* `name`           - Name of the node.
* `cluster_name`   - Name of the cluster.
* `server`         - Server version string.
* `versions`       - Versions of the internal libraries.
* `options`        - Currently active options.
* `nodes`          - List of cluster nodes.


## List Nodes

You can explicitly list all nodes in the cluster by using the
[Indices API]({{ '/docs/reference-guide/indices' | relative_url }}) and
a [Drill Selector]({{ '/docs/exploration#drill-selector' | relative_url }}):

{% capture req %}

```json
GET /.nodes
```
{% endcapture %}
{% include curl.html req=req %}


## Monitoring

You can retrieve information about the Xapiand server usage and state, by using
the `:metrics` endpoint.

{% capture req %}

```json
GET /:metrics
```
{% endcapture %}
{% include curl.html req=req %}

This returns a [Prometheus](https://prometheus.io){:target="_blank"}
compatible response with a bunch of useful metrics.
