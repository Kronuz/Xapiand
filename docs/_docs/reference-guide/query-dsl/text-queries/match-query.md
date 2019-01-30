---
title: Match Query
---

The text provided is analyzed and the analysis process constructs a boolean query from the provided text. The default boolean clauses is `OR`. The following request the text will be split and join with `OR` operator to create a query:

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

{: .note .unreleased}
**_Unimplemented Feature!_**<br>
The keyword to set operator hasn’t yet been implemented... Pull requests are welcome!
[Pull requests are welcome!]({{ site.repository }}/pulls)
