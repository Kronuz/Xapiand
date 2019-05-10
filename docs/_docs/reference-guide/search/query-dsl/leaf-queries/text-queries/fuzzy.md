---
title: Fuzzy Query
short_title: Fuzzy
---

The Fuzzy Query generates matching terms that are within the maximum edit
distance specified in `_fuzziness` (defaults to 2) and then checks the term
dictionary to find out which of those generated terms actually exist in the
index. The final query uses up to `_max_expansions` (defaults to 50) matching
terms.


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


## Parameters

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
QueryDSL parameters are not implemented yet.
[Pull requests are welcome!]({{ site.repository }}/pulls)

The following parameters are accepted by _Boolean_ fields:

|--------------------------------------|-------------------------------------------------------|
| `_fuzziness`                         | The maximum edit distance. Defaults to `2`.           |
| `_max_expansions`                    | The maximum number of terms that the fuzzy query will expand to. Defaults to `50`. |
| `_prefix_length`                     | The number of initial characters which will not be "fuzzified". This helps to reduce the number of terms which must be examined. Defaults to `0`. |


{: .note .warning }
**_Warning_**<br>
This query can be very heavy if `_prefix_length` is set to `0` and if
`_max_expansions` is set to a high number. It could result in every term
in the index being examined!
