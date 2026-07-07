---
title: "Tests"
---

* [**_Check Datatypes_**](/Xapiand/tests/datatypes/check-data-types)
* [**_Check DateType_**](/Xapiand/tests/datatypes/date-type)
* [**_Global terms_**](/Xapiand/tests/globalterms/global-terms)
* [**_Namespace_**](/Xapiand/tests/namespace/namespace)
* [**_Null type_**](/Xapiand/tests/nulltype/null-type)
* [**_Replace Value by Null_**](/Xapiand/tests/nulltype/replace-null)
* [**_Ignore Comments_**](/Xapiand/tests/comments/ignore-comments)
* [**_Create Schema_**](/Xapiand/tests/schema/create-schema)
* [**_Define Prefix_**](/Xapiand/tests/schema/define-prefix)
* [**_Define Slot_**](/Xapiand/tests/schema/define-slot)
* [**_Empty Value_**](/Xapiand/tests/schema/empty-value)
* [**_Range Search_**](/Xapiand/tests/schema/range-search)
* [**_UUID Field_**](/Xapiand/tests/schema/uuid-field)
* [**_Arrays_**](/Xapiand/tests/datastructures/arrays)
* [**_Objects_**](/Xapiand/tests/datastructures/objects)

### Reserved Subfield

<!-- e2e:begin
```rest
PUT /test/reserved_subfield/doc

{
  "name": {
    "_type": "text",
    "_reserved": "this is reserved, should fail"
  }
}
```
---
description: Index Misuse Reserved Subfield
---

```js
pm.test("Response is success", function() {
  pm.expect(pm.response.code).to.equal(400);
});
```
e2e:end -->
