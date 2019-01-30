---
title: Partial Query
---

 This is intended for use with "incremental search" systems, which don't wait for the user to finish typing their search before displaying an initial set of results. For example, in such a system a user would enter a search, and the system would display a new set of results after each letter, or whenever the user pauses for a short period of time (or some other similar strategy). The keyword `_wildcard` allows prefix matches on the last term in the text which matches any number of trailing characters, so wildc* would match wildcard, wildcarded, wildcards, wildcat, wildcats, etc.


{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_wildcard": {
      "favoriteFruit": "ba"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

A similar effect could be obtained simply by enabling the wildcard matching option, and appending a "*" character to each query string.
