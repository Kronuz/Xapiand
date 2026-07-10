---
title: "Cluster"
---

The most basic kind of information you can get is the cluster information
from the root index and the [Indexes API](/Xapiand/reference-guide/indexes):

```rest
GET /
```

<!-- e2e:begin
```js
pm.test("cluster name is Xapiand", function () {
  pm.expect(pm.response.json().cluster_name).to.eql("Xapiand");
});
```
e2e:end -->

The response contains:

* `name`           - Name of the node.
* `cluster_name`   - Name of the cluster.
* `server`         - Server version string.
* `versions`       - Versions of the internal libraries.
* `options`        - Currently active options.
* `nodes`          - List of cluster nodes.

## List Nodes

You can explicitly list all nodes in the cluster by using the
[Indexes API](/Xapiand/reference-guide/indexes) and
a [Drill Selector](/Xapiand/exploration#drill-selector):

```rest
GET /.nodes
```

<!-- e2e:begin
```js
pm.test("lists at least one active node", function () {
  var nodes = pm.response.json();
  pm.expect(nodes.length).to.be.above(0);
  pm.expect(nodes[0].active).to.eql(true);
});
```
e2e:end -->

## Monitoring

You can retrieve information about the Xapiand server usage and state, by using
the `:metrics` endpoint.

```rest
GET /:metrics
```

<!-- e2e:begin
```js
pm.test("returns Prometheus metrics", function () {
  pm.expect(pm.response.text()).to.include("xapiand_");
});
```
e2e:end -->

This returns a [Prometheus](https://prometheus.io)
compatible response with a bunch of useful metrics.
