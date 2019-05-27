---
title: Geospatial Datatype
short_title: Geospatial
---

Fields of type geo_point accept latitude-longitude pairs, which can be used:

* to find geo-points within a polygon, or a certain distance of a central point.
* to aggregate documents geographically or by distance from a central point.
* to sort documents by distance.

Available data types:

- [Point](#point)
- [Circle](#circle)
- [Convex Polygon](#convex-polygon)
- [Convex Hull](#convex-hull)


## Point

Use `_point`.

{% capture req %}

```json
UPDATE /bank/1

{
  "checkin": {
    "_point": {
      "_longitude": -80.31727,
      "_latitude": 25.67927
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Circle

Use `_circle`.

{% capture req %}

```json
UPDATE /bank/2

{
  "neighborhood": {
    "_circle": {
      "_longitude": -101.601563,
      "_latitude": 18.885498,
      "_radius": 1000
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The example show circle with radius of **1000 meters**.


## Convex Polygon

Use `_polygon`.

{% capture req %}

```json
UPDATE /bank/3

{
  "neighborhood": {
    "_polygon": {
      "_latitude": [
        41.502530,
        41.507152,
        41.506734,
        41.502121
      ],
      "_longitude": [
        -74.015237,
        -74.015061,
        -74.009160,
        -74.009489
      ]
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .warning }
_**Concave Polygons**_<br>
Note you _**must**_ provide [convex polygons](https://en.wikipedia.org/wiki/Convex_polygon){:target="_blank"}; at the moment, [concave polygons](https://en.wikipedia.org/wiki/Concave_polygon){:target="_blank"}
are not supported.


## Convex Hull

Use `_chull` to calculate the convex hull (chull) for the coordinates using the
[Graham Scan Algorithm](https://en.wikipedia.org/wiki/Graham_scan#Algorithm){:target="_blank"}:

{% capture req %}

```json
UPDATE /bank/4

{
  "neighborhood": {
    "_chull": {
      "_longitude": [
        -109.034104,
        -102.026812,
        -102.026812,
        -109.034104
      ],
      "_latitude": [
        40.993201,
        40.993201,
        36.993722,
        36.993722
      ]
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

This is the set of coordinates generate for the convex hull for the above example:

| Latitude  | Longitude | Height   |
|----------:|----------:|---------:|
| -0.166828 | -0.783064 | 0.599148 |
| -0.261113 | -0.756863 | 0.599148 |
| -0.246869 | -0.715576 | 0.653457 |
| -0.157728 | -0.740349 | 0.653457 |
| -0.166828 | -0.783064 | 0.599148 |


## Extended Well-Known Text

Use `_ewkt` to represent shapes using the Extended Well-Known Text (EWKT)
format. The following shapes are allowed types:

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

The following example indexes a point with _latitude_ of **41.50343** and
_longitude_ of **-74.01042**.

{% capture req %}

```json
UPDATE /bank/5

{
  "checkin": {
    "_ewkt": "POINT(-74.01042 41.50343)"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
**_Caution_**<br>
Notice that for points in EWKT expressions, the correct coordinate order is
`POINT(longitude latitude)`, "longitude" first.


## Accepted Coordinates

Additionally to the object formats exemplified above, coordinates also accept
array formats and some text formats.

Here are examples of formats that work:

* Text with degrees, minutes, and seconds (DMS): `41°30'12.348"N 74°0'37.512"W`
* Text with degrees and decimal minutes (DMM): `41 30.2058, -74 0.6252`
* Text with decimal degrees (DD): `41.50343, -74.01042`
* Extended Well-Known Text (EWKT): `POINT(-74.01042 41.50343)`
* Array of longitude and latitude pairs: `[ -74.01042, 41.50343 ]`

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

For example, the following example indexes a point using an array of
`[ longitude, latitude ]`:

{% capture req %}

```json
UPDATE /bank/6

{
  "checkin": [ -74.01042, 41.50343 ]
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
**_Caution_**<br>
Notice that for points in array mode, the correct coordinate order is
`[ longitude, latitude]`, "longitude" first.


## Accuracy

Xapiand handles numerical ranges by **trie indexing** numerical values in a
special string-encoded format with variable precision.

All numerical (and also dates, times and geospatial) values are converted to
lexicographic sortable string representations and indexed with different
precisions. A range of values is divided recursively into multiple intervals
for searching: The center of the range is searched only with the lowest possible
precision in the **trie**, while the boundaries are matched more exactly.

Default accuracy in geospatial fields is:

```json
[ 3, 5, 8, 10, 12, 15 ]
```

This means the precisions for geospatial values will be calculated for each of
those _HTM_ levels.

See [Hierarchical Triangular Mesh](htm) to find how _HTM_ levels work.
