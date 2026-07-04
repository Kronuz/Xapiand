---
title: Dynamic Field Names
---

Sometimes field names are dynamic: every document may introduce a different key
(user-supplied tags, attribute maps, file-system paths, and other tree-like
structures whose names are many and not known in advance). Adding each such name
as its own concrete field would make the schema grow without bound.

Dynamic field names solve this by enabling `_namespace` on the containing field.
The varying names underneath no longer add concrete fields to the schema; they
are all indexed under a single shared namespaced field and remain searchable by
their path.

{% capture req %}

```json
PUT /test_dynamic_names/1

{
  "tags": {
    "_namespace": true,
    "colors": {
      "primary": "red",
      "accent": "blue"
    },
    "size": "large"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

No matter how many different keys later documents put under `tags`, the schema
gains a single namespaced `tags` field rather than one field per key. Any path is
directly queryable:

{% capture req %}

```json
SEARCH /test_dynamic_names/

{
  "_query": {
    "tags.colors.primary": "red"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Dynamic field name is queryable by path", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
{% endcomment %}

By default every level of the path is queryable on its own (partial paths), so a
shallower path also matches:

{% capture req %}

```json
SEARCH /test_dynamic_names/

{
  "_query": {
    "tags.size": "large"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}

```js
pm.test("Partial path is queryable", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
{% endcomment %}

Dynamic field names are built on
[Namespaces]({{ '/docs/reference-guide/schemas/namespaces' | relative_url }}); see
that page for `_partial_paths` and datatype rules, and
[Namespace Queries]({{ '/docs/reference-guide/search/query-dsl/leaf-queries/namespace-queries' | relative_url }})
for how to query them.
