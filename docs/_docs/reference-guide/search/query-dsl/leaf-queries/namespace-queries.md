---
title: Namespace Queries
short_title: Namespaced
---

Namespaces are enabled by using the `_namespace` option during the schema
creation, as explained in the [Namespaces]({{ '/docs/reference-guide/schemas/namespaces' | relative_url }})
section.

Searching can be done either specifying nesting field names objects or by using
[Field Expansion]({{ '/docs/reference-guide/api/#field-expansion' | relative_url }})
and joining the field names with _dot_ ('`.`'):

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "style": {
      "clothing": "*"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "style.hairstyle": "afro"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "style.clothing.footwear": "casual shoes"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Partial Paths

By default namespaced fields keep information for each level in the path. For
example, you can also query `"footwear"` while skipping `"clothing"`, so
queries like the following also work:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "style.footwear": "casual shoes"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: sort=_id
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Namespace count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("Namespace size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("Namespace values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [1, 2, 6, 13, 17, 20, 23, 24, 25, 26];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
{% endcomment %}
