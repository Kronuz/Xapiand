---
title: "Dynamic Field Names"
---

Sometimes field names are dynamic: every document may introduce a different key
(user-supplied tags, attribute maps, and other structures whose names are many
and not known in advance). Adding each such name as its own concrete field would
make the schema grow without bound.

Dynamic field names solve this by enabling `_namespace` on the containing field.
The varying names underneath no longer add concrete fields to the schema; they
are all folded into a single shared field and stay searchable by name.

```rest
PUT /test_dynamic_names/1?commit

{
  "tags": {
    "_namespace": true,
    "color": "red",
    "size": "large",
    "shape": "round"
  }
}
```

No matter how many different keys later documents put under `tags`, the schema
gains a single namespaced `tags` field rather than one concrete field per key.
Each key stays directly queryable by its name:

```rest
SEARCH /test_dynamic_names/

{
  "_query": {
    "tags.color": "red"
  }
}
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Dynamic field name is queryable by name", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(1);
});
```
e2e:end -->

When the dynamic names are not a flat set of keys but a nested tree, and you need
to match them by path (with partial paths, datatype rules, and dedicated query
operators), that is what
[Namespaces](/Xapiand/reference-guide/schemas/namespaces) add
on top of dynamic field names; see that page and
[Namespace Queries](/Xapiand/reference-guide/search/query-dsl/leaf-queries/namespace-queries)
for the details.
