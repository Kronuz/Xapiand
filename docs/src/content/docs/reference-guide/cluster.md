---
title: "Cluster"
---

The most basic kind of information you can get is the cluster information
from the root index and the [Indices API](/Xapiand/reference-guide/indices):

```rest
GET /
```

The response contains:

* `name`           - Name of the node.
* `cluster_name`   - Name of the cluster.
* `server`         - Server version string.
* `versions`       - Versions of the internal libraries.
* `options`        - Currently active options.
* `nodes`          - List of cluster nodes.

## List Nodes

You can explicitly list all nodes in the cluster by using the
[Indices API](/Xapiand/reference-guide/indices) and
a [Drill Selector](/Xapiand/exploration#drill-selector):

```rest
GET /.nodes
```

## Monitoring

You can retrieve information about the Xapiand server usage and state, by using
the `:metrics` endpoint.

```rest
GET /:metrics
```

This returns a [Prometheus](https://prometheus.io)
compatible response with a bunch of useful metrics.
