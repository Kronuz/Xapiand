---
title: Geospatial Query
---

Geospatial allow searching with arbitrary geo shapes such as rectangles, polygons, circles and points.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
  "checkin":{
    "_point": {
        "_longitude": -74.01042,
        "_latitude": 41.50343
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Replace `_point` with any of `_circle`, `_polygon`, `_convex`, `_chull`, `_multipoint`, `_multicircle`, `_multiconvex`, `_multipolygon` and `_multichull`

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
  "checkin":{
    "_circle": {
      "_longitude": -101.601563,
      "_latitude": 18.885498,
      "_radius": 10000
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The `_radius` in the above example is on meters


Shapes can be represented using Well-Known Text (WKT) format. The following shapes are allowed types:

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

The following example search a point with longitude of -74.01042 and latitude 41.50343.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "checkin":{
      "_ewkt": "POINT(-74.01042 41.50343)"
    }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .info}
**_WKT expression_**<br>
Notice the correct coordinate order is longitude, latitude (X, Y)