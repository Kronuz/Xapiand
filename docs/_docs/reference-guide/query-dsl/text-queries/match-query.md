---
title: Match Query
---

The text provided is analyzed and the analysis process constructs a boolean
query from the provided text. The default boolean operator is `OR`, so the
following request the text will be split and join with the `OR` operator to
create a query:

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


## Default Operator

The default operator to be used can be set by using the `default_operator`
parameter. It can be `AND` or `OR`. Defaults to `OR`.

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
[Pull requests are welcome!]({{ site.repository }}/pulls)
