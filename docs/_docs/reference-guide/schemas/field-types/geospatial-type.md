---
title: Geospatial Datatype
short_title: Geospatial
---

Fields of type geo_point accept latitude-longitude pairs, which can be used:

* to find geo-points within a polygon, or a certain distance of a central point.
* to aggregate documents geographically or by distance from a central point.
* to sort documents by distance.


### Example:

{% capture req %}

```json
MERGE /bank/1?pretty

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

In the above example, taken from our example dataset, the field "checkin" is
a geospatial point but could be any of:

* [Point](#)
* [Polygon](#polygon)
* [Circle](#circle)
* `Covex`
* [Chull](#convex-hull)
* `Multipoint`
* `Multipcircle`
* `Multiconvex`
* `Multipolygon`
* `Multichull`


## Polygon

{% capture req %}

```json
MERGE /bank/1?pretty

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


## Circle

{% capture req %}

```json
MERGE /bank/1?pretty
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


## Convex Hull

Calculate the convex hull for the coordinates using the
[Graham Scan Algorithm](https://en.wikipedia.org/wiki/Graham_scan#Algorithm){:target="_blank"}:

{% capture req %}

```json
MERGE /bank/1?pretty
{
  "neighborhood": {
    "_chull": {
      "_longitude": [
        -109.034104,
        -102.026812,
        -102.026812,
        -109.034104
      ],
      "_latitude":[
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
|-----------+-----------+----------+
| -0.166828 | -0.783064 | 0.599148 |
| -0.261113 | -0.756863 | 0.599148 |
| -0.246869 | -0.715576 | 0.653457 |
| -0.157728 | -0.740349 | 0.653457 |
| -0.166828 | -0.783064 | 0.599148 |
