---
title: Partial Query
short_title: Partial
---

This is intended for use with "incremental search" systems, which don't wait
for the user to finish typing their search before displaying an initial set of
results. For example, in such a system a user would start typing the query and
the system would immediately display a new set of results after each letter
keypress, or whenever the user pauses for a short period of time (or some other
similar strategy).

This allows for prefix matches, matching any number of trailing characters, so,
for instance, _wildc_* would match *wildc*ard, *wildc*arded, *wildc*ards,
*wildc*at, *wildc*ats, etc.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "favoriteFruit": "ba*"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


The same effect could be obtained by using the `_wildcard` keyword as part of
the QueryDSL:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "favoriteFruit": {
      "_partial": "ba"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

