---
title: Stop Words
---

Xapian supports a stop word list, which allows you to specify what words
should be removed from a query before processing. This list can be overridden
or stop words can still be searched for if desired, but by default any words
in the active stop words list will not be searched for.

### Example

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "these days are few and far between"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

We can look this individual document, the field personality do not include the
entire phrase we look in the query that is because remove all the stop words
and only use "days" for the query.


## Searching of Stop Words

Stop words can be searched by using [Love and Hate Modifiers](love-and-hate-modifiers)
(by adding `+` to the desired stop word) or by using an empty set of stopwords
in the `_stopwords` keyword:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "+these days +are +few +and +far +between"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

The above example is equivalent to:

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": {
      "_value": "these days are few and far between",
      "_stopwords": []
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
