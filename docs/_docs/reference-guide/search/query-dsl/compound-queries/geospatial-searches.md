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


## Circle

Although you certainly can use `_circle` the same way we used [Point](#point),
to directly match an specific circle, you will probably more likely need to find
documents within the given circle. For that, the `_in` keyword is needed:

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

Use `_polygon`.

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
Notice you _**must**_ provide convex polygons; at the moment, concave polygons
are not supported.


## Convex

Use `_convex`.

{: .note .construction }
_This section is a **work in progress**..._


## Chull

Use `_chull`.

{: .note .construction }
_This section is a **work in progress**..._


## Multipoint

Use `_multipoint`.

{: .note .construction }
_This section is a **work in progress**..._


## Multicircle

Use `_multicircle`.

{: .note .construction }
_This section is a **work in progress**..._


## Multiconvex

Use `_multiconvex`.

{: .note .construction }
_This section is a **work in progress**..._


## Multipolygon

Use `_multipolygon`.

{: .note .construction }
_This section is a **work in progress**..._


## Multichull

Use `_multichull`.

{: .note .construction }
_This section is a **work in progress**..._


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

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

Here are examples of text formats that work:

* Degrees, minutes, and seconds (DMS): `41°30'12.348"N 74°0'37.512"W`
* Degrees and decimal minutes (DMM): `41 30.2058, -74 0.6252`
* Decimal degrees (DD): `41.50343, -74.01042`
* Extended Well-Known Text (EWKT): `POINT(-74.01042 41.50343)`

Here are some tips for formatting your coordinates so they work:

* Use the degree symbol instead of "`d`".
* Use double quoted symbol (`"`) instead of two single quoted symbols
  (and escape, in JSON strings).
* Use periods as decimals, not commas.
  - Incorrect: `41,50343, -74,01042`.
  - Correct: `41.50343, -74.01042`.
* List your latitude coordinates before longitude coordinates.
* Check that the first number in your latitude coordinate is between -90 and 90.
* Check that the first number in your longitude coordinate is between -180 and 180.
