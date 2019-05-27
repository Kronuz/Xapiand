---
title: Geospatial Searches
short_title: Geospatial
---

Geospatial allow searching within arbitrary "geo-shapes" such as rectangles,
polygons, circles, points etc.


## Point

Use `_point` to directly match an specific point:

{% capture req %}

```json
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
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
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
{% endcomment %}


## Circle

Although you certainly can use `_circle` the same way we used [Point](#point),
to directly match an specific exact circle, you will probably more likely need
to find documents _within_ the given circle. For that, the `_in` keyword is
needed:

{% capture req %}

```json
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
{% endcapture %}
{% include curl.html req=req %}

The `_radius` in the example is in meters.

{: .note .caution }
**_Caution_**<br>
For searching **inside** the given circle you _**must**_ use `_in` keyword.
Otherwise you'd be searching for the circle itself, not what's in it.


## Polygon

Similarly to Circle, above, you can use Polygon to find documents _within_ the
given convex polygon:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "checkin": {
      "_in": {
        "_polygon": {
          "_latitude": [
            41.502530,
            41.507152,
            41.506734,
            41.502121,
          ],
          "_longitude": [
            -74.015237,
            -74.015061,
            -74.009160,
            -74.009489,
          ]
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
_**Concave Polygons**_<br>
Notice you _**must**_ provide [convex polygons](https://en.wikipedia.org/wiki/Convex_polygon){:target="_blank"}; at the moment, [concave polygons](https://en.wikipedia.org/wiki/Concave_polygon){:target="_blank"}
are not supported.


## Extended Well-Known Text

Use `_ewkt`.

Shapes can be represented using the Extended Well-Known Text (EWKT) format.
The following shapes are allowed types:

* POINT         - Finds documents with the given point
* CIRCLE        - Finds documents inside or with the given circle.
* CONVEX        - Finds documents inside or with the given convex.
* POLYGON       - Finds documents inside or with the given polygon.
* CHULL         - Finds documents inside or with the given convex hull.
* MULTIPOINT    - Finds documents inside the given vector of points.
* MULTIPCIRCLE  - Finds documents inside the given vector of circles.
* MULTICONVEX   - Finds documents inside the given vector of convexs.
* MULTIPOLYGON  - Finds documents inside the given vector of polygons.
* MULTICHULL    - Finds documents inside the given vector of convexs hull.

The following example search a point with _latitude_ of **41.50343** and
_longitude_ of **-74.01042**.

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "checkin": {
      "_ewkt": "POINT(-74.01042 41.50343)"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
**_Caution_**<br>
Notice that for points in EWKT expressions, the correct coordinate order is
`(longitude, latitude)`, "longitude" first.


## Accepted Coordinates

Additionally to the object formats exemplified above, coordinates also accept
array formats and text formats.

Here are examples of formats that work:

* Degrees, minutes, and seconds (DMS): `41°30'12.348"N 74°0'37.512"W`
* Degrees and decimal minutes (DMM): `41 30.2058, -74 0.6252`
* Decimal degrees (DD): `41.50343, -74.01042`
* Extended Well-Known Text (EWKT): `POINT(-74.01042 41.50343)`
* JSON array of longitude and latitude pairs: `[ -74.01042, 41.50343 ]`

Here are some tips for formatting your coordinates so they work:

* Use the degree symbol instead of "`d`".
* Use double quoted symbol (`"`) instead of two single quoted symbols
  (and escape, in JSON strings).
* Use _periods_ as decimals, not _commas_.
  - Incorrect: `41,50343, -74,01042`.
  - Correct: `41.50343, -74.01042`.
* List your latitude coordinates before longitude coordinates.
* Check that the first number in your latitude coordinate is between -90 and 90.
* Check that the first number in your longitude coordinate is between -180 and 180.

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
DMS, DMM and DD text formats  haven't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)
