---
title: Wildcard Query
short_title: Wildcard
---

This allows for prefix matches, matching any number of trailing characters, so,
for instance, `"_partial": "wildc"` or `"wildc*"` would match _**wildc**ard_,
_**wildc**arded_, _**wildc**ards_, _**wildc**at_, _**wildc**ats_, etc.

{: .note .tip }
This is a bit different from [Partial Query](../partial). Wildcard queries
do not find the queried word as a whole. Partial is intended for "incremental
search".

### Example

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": {
      "_wildcard": "ba"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

A similar effect could be obtained by using the wildcard ("`*`") character
as part of the query text:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": "ba*"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
