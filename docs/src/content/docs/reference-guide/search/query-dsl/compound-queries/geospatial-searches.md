---
title: "Geospatial Searches"
---

Geospatial allow searching within arbitrary "geo-shapes" such as rectangles,
polygons, circles, points etc.

for querying geospatial searches, you can use any of the supported shapes:

* [Point](/Xapiand/reference-guide/schemas/field-types/geospatial-type#point)
* [Circle](/Xapiand/reference-guide/schemas/field-types/geospatial-type#circle)
* [Convex Polygon](/Xapiand/reference-guide/schemas/field-types/geospatial-type#convex-polygon)
* [Convex Hull](/Xapiand/reference-guide/schemas/field-types/geospatial-type#convex-hull)
* [EWKT](/Xapiand/reference-guide/schemas/field-types/geospatial-type#extended-well-known-text)

## Exact Matching

Use `_point` to directly match an specific exact point:

```rest
SEARCH /bank/

{
  "_query": {
    "checkin": {
      "_point": {
        "_latitude": 41.50343,
        "_longitude": -74.01042
      }
    }
  }
}
```

<!-- e2e:begin
---
params: sort=_id
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Geospatial Point count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("Geospatial Point size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("Geospatial Point values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
e2e:end -->

## Match Within

Although you certainly can use any of the geospatial shapes in the same way we
used Point for [Exact Matching](#exact-matching), to directly match an specific
exact shape, you will probably more likely need to find documents _within_ the
given shape. For that, the `_in` keyword is needed:

```rest
SEARCH /bank/

{
  "_query": {
    "checkin": {
      "_in": {
        "_circle": {
          "_latitude": 41.50830,
          "_longitude": -73.97696,
          "_radius": 5000
        }
      }
    }
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Match Within results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(4);
  pm.expect(jsonData.hits.length).to.equal(4);
});
```
e2e:end -->

In the example above we are searching inside a circle with it's center is given
by `_latitude`, `_longitude` and a `_radius` (see [Supported Distance Units](/Xapiand/reference-guide/schemas/field-types/geospatial-type/#distance-units)).

:::hint[Caution]{.caution}
For searching **inside** the given shape you _**must**_ use `_in` keyword.
Otherwise you'd be searching for the shape itself, not what's in it.
:::
