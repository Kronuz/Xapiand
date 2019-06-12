---
title: Schemas
---

A schema is the definition of how a document, and the fields it contains, are
stored and indexed. For instance, use schemas to define:

* which string fields should be treated as full text fields.
* which fields contain numbers, dates, or geolocations.
* which fields should be indexed globally.
* custom rules to control the typing for dynamically added fields.


## [Field Types](field-types)

Each field has a data type which can be:

* a simple type like `text`, `string`, `keyword`, `datetime`, `float` or `boolean`.
* a sub-type which supports the hierarchical nature of JSON such as `object` or `array`.
* or a specialised type like `geospatial`.


## [Dynamic Typing](dynamic-typing)
Field types do not need to be defined before being used. Thanks to dynamic
typing, new field names will be added automatically, just by indexing a
document. New fields can be added both to the top-level and to inner object and
nested fields.

The dynamic typing rules can be configured to customise the type that is used
for new fields.


## [Explicit Types](explicit-types)

You know more about your data than Xapiand can guess, so while dynamic
typing can be useful to get started, at some point you will want to specify
your own explicit types.

You can create field types when you create an index, and you can add fields to
an existing index with the [Create Index API]({{ '/docs/reference-guide/indices/create-api' | relative_url }}).


## [Casting Types](casting-types)

{: .note .construction }
_This section is a **work in progress**..._


## [Accuracy](accuracy)

{: .note .construction }
_This section is a **work in progress**..._


## [Indexing Mode](indexing-mode)

{: .note .construction }
_This section is a **work in progress**..._

* `none`, `all`
* `field`, `field_terms`, `field_values`
* `global`, `global_terms`, `global_values`
* `terms`, `values`


## [Dynamic Field Names](dynamic-field-names)

{: .note .construction }
_This section is a **work in progress**..._


## [Namespaces](namespaces)

{: .note .construction }
_This section is a **work in progress**..._


## [Foreign Schemas](foreign-schemas)

When explicitly used, Foreign Schemas can establish where the schema information
will be stored. For example, when a set of indices all share the same schema
it'd be beneficial to have a single shared schema. This can be accomplished by
setting the foreign schema of all such indexes to point to a document containing
the schema.


{: .note .info }
**_Updating Existing Field Types_**<br>
Other than where documented, **existing field types cannot be updated**.
Changing the name or the type of a field would mean invalidating already indexed
documents. Instead, you should create a new index with the correct field types
and reindex your data into that index.


{: .note .info }
**_One Index, One Document Type_**<br>
In Xapiand, one index can contain one document type. E.g. Instead of
storing two document types in a single index, one should store tweets in the
`tweets` index and users in the `users` index. Indices are completely
independent of each other and so there will be no conflict of field types
between indices.
