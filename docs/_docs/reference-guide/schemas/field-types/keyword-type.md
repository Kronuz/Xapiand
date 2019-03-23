---
title: Keyword Datatype
short_title: Keyword
---

A field to index structured content such as email addresses, hostnames, status codes, zip codes or tags.

They are typically used for filtering (Find me all blog posts where status is published), for sorting, and for aggregations. Keyword fields are only searchable by their exact value.

If you need to index full text content such as email bodies or product descriptions, it is likely that you should rather use a text field. By default every field in the document with text value is interpreted as type `text` so you must explicitly use `keyword` if you want a field not be `text`.

If you are following the documentation examples you may notice that the field "employer" is indexed as `text` before but this time we index the field as `keyword`


{% capture req %}

```json
UPDATE /bank/1

{
  "username": {
    "_type": "keyword",
    "_value": "mlee"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
**_Caution_**<br>
Notice we are using the same index "bank" as before, if you already indexed at this index with previous examples you are going to get a error for change types for the field  "employer" from `text` to `keyword`


## Parameters

The following parameters are accepted by _Keyword_ fields:

|---------------------------------------|-----------------------------------------------------------------------------------------|
| `_value`                              | The value for the field. (Only used at index time).                                     |
| `_index`                              | The mode the field will be indexed as: `"none"`, `"field_terms"`, `"field_values"`, `"field_all"`, `"field"`, `"global_terms"`, `"global_values"`, `"global_all"`, `"global"`, `"terms"`, `"values"`, `"all"`. (The default is `"field_all"`). |
| `_slot`                               | The slot number. (It's calculated by default).                                          |
| `_prefix`                             | The prefix the term is going to be indexed with. (It's calculated by default)           |
| `_weight`                             | The weight the term is going to be indexed with.                                        |
