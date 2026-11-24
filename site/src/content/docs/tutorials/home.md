---
title: "Tutorials"
---

A tutorial walks through one scenario from beginning to end on a running node:
it loads a sample dataset, runs the requests the scenario needs and shows their
answers. The [Reference Guide](/Xapiand/reference-guide/api) documents each
request on its own; a tutorial combines them.

Like every page of this site, a tutorial's requests run as part of the
documentation's end-to-end tests, so its answers are the node's (see
[Coverage](/Xapiand/tests/coverage)).

* **[Spatial Search](/Xapiand/tutorials/spatial-search)**: loads 1,000 US cities
  with their location and population, finds the large cities within 20 km of a
  point, and sorts them by their distance to another point. It uses
  [geospatial fields](/Xapiand/reference-guide/schemas/field-types/geospatial-type),
  a [range](/Xapiand/reference-guide/search/query-dsl/compound-queries/range-searches)
  and a [geospatial search](/Xapiand/reference-guide/search/query-dsl/compound-queries/geospatial-searches).
