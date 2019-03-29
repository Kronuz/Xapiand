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

In the above example, taken from our
[example dataset]({{ '/docs/exploration/#sample-dataset' | relative_url }}),
the field "checkin" is a geospatial point but could be any of:

- [Accuracy](#accuracy)
- [Polygon](#polygon)
- [Circle](#circle)
- [Convex Hull](#convex-hull)


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


## Polygon

{% capture req %}

```json
UPDATE /bank/1

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
UPDATE /bank/1
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
UPDATE /bank/1
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
|----------:|----------:|---------:|
| -0.166828 | -0.783064 | 0.599148 |
| -0.261113 | -0.756863 | 0.599148 |
| -0.246869 | -0.715576 | 0.653457 |
| -0.157728 | -0.740349 | 0.653457 |
| -0.166828 | -0.783064 | 0.599148 |
