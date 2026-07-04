---
title: Explicit Types
---

You know more about your data than Xapiand can guess, so while
[Dynamic Typing]({{ '/docs/reference-guide/schemas/dynamic-typing' | relative_url }})
is useful to get started, at some point you will want to specify your own
explicit types.

Types are declared inside the `_schema` object when you create an index (or the
first time a field is seen), giving each field a `_type`:

{% capture req %}

```json
PUT /test_explicit_types/

{
  "_schema": {
    "name": {
      "_type": "text"
    },
    "age": {
      "_type": "positive"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

Once declared, the type is enforced. Documents that fit the schema are indexed
as usual:

{% capture req %}

```json
PUT /test_explicit_types/1

{
  "name": "Jane Austen",
  "age": 41
}
```
{% endcapture %}
{% include curl.html req=req %}

And the field is queryable with the declared type:

{% capture req %}

```json
SEARCH /test_explicit_types/

{
  "_query": {
    "name": "Jane"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

A value that cannot be coerced into the declared type (for instance a negative
`age` for a `positive` field) is rejected rather than silently reinterpreted.

{: .note .info }
**_Only for New Fields_**<br>
`_type` can only be set the first time a field is created. Attempting to change
the type of an existing field during an update results in a Bad Request error.

{% comment %}

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Explicit-typed field is searchable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
{% endcomment %}
