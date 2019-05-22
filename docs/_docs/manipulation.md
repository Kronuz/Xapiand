---
title: Data Manipulation
---

Xapiand provides data manipulation and search capabilities in near real time.
By default, you can expect a one second delay (refresh interval) from the time
you index/update/delete your data until the time that it appears in your search
results. This is an important distinction from other platforms like SQL wherein
data is immediately available after a transaction is completed.

## Indexing/Replacing Documents

We've previously seen how we can index a single document. Let's recall that
command again:

{% capture req %}

```json
PUT /twitter/user/John

{
  "name": "John Doe"
}
```
{% endcapture %}
{% include curl.html req=req %}

The above will index the specified document into the user index, with the ID of
`John`. If we then executed the above command again with a different (or same)
document, Xapiand will replace (i.e. reindex) a new document on top of the
existing one with the ID of `John`:

{% capture req %}

```json
PUT /twitter/user/John

{
  "name": "John"
}
```
{% endcapture %}
{% include curl.html req=req %}

The above completely overwrites the existent document with the ID of `John` with
the new one.

If, on the other hand, we use a different ID, a new document will be indexed
and the existing document(s) already in the index remains untouched.

{% capture req %}

```json
PUT /twitter/user/Jane

{
  "name": "Jane Doe",
  "age": 20
}
```
{% endcapture %}
{% include curl.html req=req %}

The above indexes a new document with an ID of `Jane`.

When indexing, the ID part is optional. If not specified, Xapiand will generate
an ID and then use it to index the document. The actual ID Xapiand generates
(or whatever we specified explicitly in the previous examples) is returned as
part of the index API call.

This example shows how to index a document without an explicit ID:

{% capture req %}

```json
POST /twitter/user/

{
  "name": "Richard Roe"
}
```
{% endcapture %}
{% include curl.html req=req %}

Note that in the above case, we are using the `POST` verb instead of `PUT`
as we're not specifying an explicit ID.


## Updating Documents

In addition to being able to index and replace documents, we can also update
documents.

This example shows how to update our previous document (ID of `John`) by adding
a _gender_ field to it and leaving the _name_ intact:

**Add gender to John:**

{% capture req %}

```json
UPDATE /twitter/user/John

{
  "gender": "male"
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
pm.test("response is merged", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.name).to.be.an('string');
  pm.expect(jsonData.gender).to.equal('male');
});
```

And the following example shows how to update the same document by adding an
_age_ field to it and at the same time changing the _name_ from simply
_"John"_ to _"Johnny Doe"_:

{% capture req %}

```json
UPDATE /twitter/user/John

{
  "name": "Johnny Doe",
  "age": 17
}
```
{% endcapture %}
{% include curl.html req=req %}


### Updating With Scripts

Updates can also be performed by using simple scripts. This example uses a
script to increment the _age_ by 5 and at the same time change the _name_:

{% capture req %}

```json
UPDATE /twitter/user/John

{
  "name": "John Doe",
  "_script": "_doc.age = _old_doc.age + 5"
}
```
{% endcapture %}
{% include curl.html req=req %}

In the above example, `_doc` is the current document and `_old_doc` refers to
the previous document that is about to be updated.


## Deleting Documents

Deleting a document is fairly straightforward. This example shows how to delete
our previous user with the ID of `Jane`:

{% capture req %}

```json
DELETE /twitter/user/Jane
```
{% endcapture %}
{% include curl.html req=req %}
