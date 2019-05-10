---
title: Fuzzy Query
short_title: Fuzzy
---

The Fuzzy Query generates matching terms that are within the maximum edit
distance specified in `_fuzziness` (defaults to 2) and then checks the term
dictionary to find out which of those generated terms actually exist in the
index. The final query uses up to `_max_expansions` (defaults to 50) matching
terms.

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
The `_fuzzyness` and `_max_expansions` parameters are not implemented yet.
[Pull requests are welcome!]({{ site.repository }}/pulls)


### Example

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": {
      "_fuzzy": "banna"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

A similar effect could be obtained by using the tilde ("`~`") character
followed optionally by the edit distance as part of the query text:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "favoriteFruit": "banna~"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .info }
Edit distance defaults to 2, so `"banna~"` and `"banna~2"` are equivalent.
