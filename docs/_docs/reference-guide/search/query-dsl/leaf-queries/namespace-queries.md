---
title: Namespace Queries
short_title: Namespaced
---

In use-cases where it's not feasible/convenient to create a new field in the
schema for each field inside a tree. Namespace enabled fields allow efficient
searching of values within nested fields. For example, this feature can be used
for tags, file-system path names, or any tree-like structure for which contained
names can be many, dynamic and/or not known in advance.

The `_namespace` parameter must be enabled during Schema creation:

```json
{
  "style": {
    "_namespace": true,
    "clothing": {
      "pants": "khakis",
      "footwear": "casual shoes"
    },
    "hairstyle": "afro"
  }, ...
}
```

The above example is the document being indexed, the parameter `_namespace`
enables the Namespace Queries functionality.

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
example, you can also query `"footwear"` while skipping `"clothing"`:

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

{: .test }

```js
pm.test("response is ok", function() {
  pm.response.to.be.ok;
});
```

{: .test }

```js
pm.test("Namespace count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

{: .test }

```js
pm.test("Namespace size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

{: .test }

```js
pm.test("Namespace values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [978, 62, 471, 485, 649, 277, 537, 602, 689, 764];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```

This behaviour can be modified by setting `_partial_paths` to `false`, when
creating the schema:

```json
{
  "style": {
    "_type": "keyword",
    "_namespace": true,
    "_partial_paths": false,
  }, ...
}
```


## Datatype

The concrete datatype for all nested objects must be of the same type as one
declared in the object enabling the `_namespace`.
