---
title: Love and Hate Query
short_title: Love and Hate
---

The `+` and `-` operators, select documents based on the presence or absence of
specified terms.

{: .note .info }
**_Note_**<br>
When using these operators, _stop words_ do not apply.


#### Example

The following matches all documents with the phrase _"adventurous nature"_ but
not _ambitious_; and:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "\"adventurous nature\" -ambitious"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{: .note .caution }
**_Caution_**<br>
One thing to note is that the behaviour of the +/- operators vary depending on
the default operator used and the above examples assume that the default (`OR`)
is used.
