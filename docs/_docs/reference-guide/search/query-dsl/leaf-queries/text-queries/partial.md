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
for instance, `"_partial": "wildc"` or `"wildc*"` would match _**wildc**ard_,
_**wildc**arded_, _**wildc**ards_, _**wildc**at_, _**wildc**ats_, etc.

{: .note .tip }
This is a bit different from [Wildcard Query](../wildcard).
Partial is intended for "incremental search".


### Example

{% capture req %}

```json
SEARCH /bank/

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
