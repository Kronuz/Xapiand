---
title: Geospatial Query
---

Geospatial allow searching within arbitrary geo shapes such as rectangles,
polygons, circles, points etc.


## Point

Use `_point` to directly match an specific point:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "checkin":{
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

Use `_circle` to match all documents with coordinates inside the given circle:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "checkin":{
      "_circle": {
        "_latitude": 41.50830,
        "_longitude": -73.97696,
        "_radius": 5000
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The `_radius` in the above example is in meters.

## Polygon

Use `_polygon`.

{: .note .construction}
_This section is a **work in progress**..._


## Convex

Use `_convex`.

{: .note .construction}
_This section is a **work in progress**..._


## Chull

Use `_chull`.

{: .note .construction}
_This section is a **work in progress**..._


## Multipoint

Use `_multipoint`.

{: .note .construction}
_This section is a **work in progress**..._


## Multicircle

Use `_multicircle`.

{: .note .construction}
_This section is a **work in progress**..._


## Multiconvex

Use `_multiconvex`.

{: .note .construction}
_This section is a **work in progress**..._


## Multipolygon

Use `_multipolygon`.

{: .note .construction}
_This section is a **work in progress**..._


## Multichull

Use `_multichull`.

{: .note .construction}
_This section is a **work in progress**..._


## Extended Well-Known Text

Use `_ewkt`.

Shapes can be represented using the Extended Well-Known Text (EWKT) format.
The following shapes are allowed types:

* POINT         - Finds documents with the point
* CIRCLE        - Finds documents inside the circle or with the circle.
* CONVEX        - Finds documents inside the convex or with the convex.
* POLYGON       - Finds documents inside the polygon or with the polygon.
* CHULL         - Finds documents inside the convex hull or with the convex hull.
* MULTIPOINT    - Finds documents inside the vector of points.
* MULTIPCIRCLE  - Finds documents inside the vector of circles.
* MULTICONVEX   - Finds documents inside the vector of convexs.
* MULTIPOLYGON  - Finds documents inside the vector of polygons.
* MULTICHULL    - Finds documents inside the vector of convexs hull.

The following example search a point with _latitude_ of **41.50343** and
_longitude_ of **-74.01042**.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "checkin":{
      "_ewkt": "POINT(-74.01042 41.50343)"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .info}
**_EWKT expression_**<br>
Notice the correct coordinate order is (longitude, latitude)


## Accepted Coordinates

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
