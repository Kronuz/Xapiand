---
title: Cluster
---

The most basic kind of information you can get is the cluster information:

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


## List Nodes

You can list all nodes in the cluster by using the `:nodes` endpoint.

{% capture req %}

```json
GET /:nodes
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
