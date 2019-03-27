---
title: Spatial Search Tutorial
permalink: /tutorials/spatial-search/
---

This is a trivial example to demonstrate Xapiand's spatial search capabilities.
Given any point, find the closest [large cities in the US](https://en.wikipedia.org/wiki/List_of_United_States_cities_by_population). For this example, a large city
incorporates places in the United States with a population of at least 100,000.


## Loading the Sample Dataset

The population and location data used in this example is from
[OpenDataSoft](https://public.opendatasoft.com/explore/dataset/1000-largest-us-cities-by-population-with-geographic-coordinates).

You can download the [sample dataset]({{ '/assets/cities.ndjson' | absolute_url }}){:download="cities.ndjson"}.
Extract it to our current directory and let's load it into our cluster as follows:

{% capture req %}

```json
RESTORE /cities/
Content-Type: application/x-ndjson

@cities.ndjson
```
{% endcapture %}
{% include curl.html req=req %}


## Searching

Let's look for the nearest big cities near **_El Cerrito_, CA**, a city
neighboring Berkeley in the San Francisco Bay Area.

![El Cerrito, CA]({{ '/assets/el_cerrito.png' | absolute_url }})

The red circle in the map has a radius of **20 km** and is at the latitude/longitude
of _El Cerrito_ **(37.9180233, -122.3198401)**. The map also shows all
**trixels** that will be searched for (to speed the query up).
See [Hierarchical Triangular Mesh]({{ '/docs/reference-guide/schemas/field-types/geospatial-type/htm' | relative_url }})
to find how _HTM **trixels**_ work.

{% capture req %}

```json
SEARCH /cities/

{
  "_query": {
    "population": {
      "_in": {
        "_range": {
          "_from": 100000
        }
      }
    },
    "location": {
      "_in": {
        "_circle": {
          "_latitude": 37.9180233,
          "_longitude": -122.3198401,
          "_radius": 20000
        }
      }
    }
  },
  "_selector": "city"
}
```
{% endcapture %}
{% include curl.html req=req %}

Sure enough, Xapiand returns a list of cities in the Bay Area, nearest first:

```json
{
  "total": 5,
  "count": 5,
  "hits": [
    "Richmond",
    "Berkeley",
    "Oakland",
    "San Francisco",
    "Vallejo"
  ]
}
```


## Sorting

It might be that you want to change the reference point for sorting purposes,
for example you still might want to get the same cities close to _El Cerrito_,
but this time sorting them by their proximity to **_San Francisco, CA_**.

The latitude/longitude of _San Francisco_ is **(37.7576171, -122.5776844)**.

{% capture req %}

```json
SEARCH /cities/

{
  "_query": {
    "population": {
      "_in": {
        "_range": {
          "_from": 100000
        }
      }
    },
    "location": {
      "_in": {
        "_circle": {
          "_latitude": 37.9180233,
          "_longitude": -122.3198401,
          "_radius": 20000
        }
      }
    }
  },
  "_sort": {
    "location": {
      "_order": "asc",
      "_value": {
        "_point": {
          "_latitude": 37.7576171,
          "_longitude": -122.5776844,
        }
      }
    }
  },
  "_selector": "city"
}
```
{% endcapture %}
{% include curl.html req=req %}

Now Xapiand returns the same cities, but now nearest to _San Francisco_ first.

```json
{
  "total": 5,
  "count": 5,
  "hits": [
    "San Francisco",
    "Oakland",
    "Richmond",
    "Berkeley",
    "Vallejo"
  ]
}
```
