---
title: Modifying Your Data
---

Xapiand provides data manipulation and search capabilities in near real time.
By default, you can expect a one second delay (refresh interval) from the time
you index/update/delete your data until the time that it appears in your search
results. This is an important distinction from other platforms like SQL wherein
data is immediately available after a transaction is completed.

## Indexing/Replacing Documents

We've previously seen how we can index a single document. Let's recall that
command again:

```json
PUT /customer/1?pretty
{
  "name": "John Doe"
}
```

The above will index the specified document into the customer index, with the
ID of 1. If we then executed the above command again with a different (or same)
document, Xapiand will replace (i.e. reindex) a new document on top of the
existing one with the ID of 1:

```json
PUT /customer/1?pretty
{
  "name": "Jane Doe"
}
```

The above changes the name of the document with the ID of 1 from "John Doe" to
"Jane Doe".

If, on the other hand, we use a different ID, a new document will be indexed
and the existing document(s) already in the index remains untouched.

```json
PUT /customer/2?pretty
{
  "name": "Jane Doe"
}
```

The above indexes a new document with an ID of 2.

When indexing, the ID part is optional. If not specified, Xapiand will generate
an ID and then use it to index the document. The actual ID Xapiand generates
(or whatever we specified explicitly in the previous examples) is returned as
part of the index API call.

This example shows how to index a document without an explicit ID:

```json
POST /customer?pretty
{
  "name": "Jane Doe"
}
```

Note that in the above case, we are using the POST verb instead of PUT since we
didn't specify an ID.


## Updating Documents

In addition to being able to index and replace documents, we can also update
documents.

This example shows how to update our previous document (ID of 1) by changing
the name field to "Jane Doe":

```json
POST /customer/1?pretty
{
  "name": "Jane Doe",
  "age": 20
}
```

This example shows how to update our previous document (ID of 1) by changing
the name field to "John Doe" and at the same time add a gender field to it:

```json
MERGE /customer/1?pretty
{
  "name": "John Doe",
  "gender": "male"
}
```

Updates can also be performed by using simple scripts. This example uses a
script to increment the age by 5:

```json
MERGE /customer/1?pretty
{
  "script": "obj.age += 5"
}
```

In the above example, `obj` refers to the current source document that is about
to be updated.

{% if site.serving %}

<!-- TODO: Implement feature -->
Xapiand provides the ability to update multiple documents given a specific
query condition (like an SQL UPDATE-WHERE statement):

```json
MERGE /customer/:search?q=*&pretty
{
  "script": "obj.age += 1"
}
```

{% endif %}


## Deleting Documents

Deleting a document is fairly straightforward. This example shows how to delete
our previous customer with the ID of 2:

```json
DELETE /customer/2?pretty
```

{% if site.serving %}

<!-- TODO: Implement feature -->
Xapiand provides the ability to delete multiple documents given a specific
query condition.

```json
DELETE /customer/:search?q=gender:male&pretty
```

{: .note .warning}
It is worth noting that it is much more efficient to delete a
whole index instead of deleting all documents using this method.

{% endif %}
