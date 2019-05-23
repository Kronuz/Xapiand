---
title: Versioning
---

Xapiand is distributed. When documents are created, updated, or deleted, the
new version of the document has to be replicated to other nodes in the cluster.
Xapiand is also asynchronous and concurrent, meaning that these replication
requests are sent in parallel, and may arrive at their destination
_out of sequence_. Xapiand needs a way of ensuring that an older version of a
document never overwrites a newer version.

To ensure an older version of a document doesn't overwrite a newer version,
every document has a `_version` number that is incremented whenever a document
is changed. Xapiand uses this `_version` number to ensure that changes are
applied in the correct order. If an older version of a document arrives after a
new version, it can simply be ignored.

For example, the following indexing command will create a document and assign
a version to it:

{% capture req %}

```json
PUT /twitter/user/Kronuz

{
  "name": "Germán Méndez Bravo"
}
```
{% endcapture %}
{% include curl.html req=req %}

This is all business as usual, the interesting bit is in the response:

```json
{
  "name": "Germán Méndez Bravo",
  "_id": "Kronuz",
  "_version": 1
}
```

Notice the `_version` returned when performing an index operation. This is now
the version associated with this particular document. If we get or search for
documents in an index, we will get the version as well.


## Optimistic Concurrency Control

Now, the interesting bits can start, as we can use the versioning feature to
perform [optimistic concurrency control](https://en.wikipedia.org/wiki/Optimistic_concurrency_control){:target="_blank"}.
We can take advantage of the `_version` number to ensure that conflicting
changes made by our application do not result in data loss. We do this by
specifying the version number of the document that we wish to change. If
that version is no longer current, our request fails.

Let’s create a new blog post:

{% capture req %}

```json
PUT /blog/1

{
  "title": "My first blog entry",
  "text": "Just trying this out..."
}
```
{% endcapture %}
{% include curl.html req=req %}

The response body tells us that this newly created document has `_version`
number `1`. Now imagine that we want to edit the document: we load its data into
a web form, make our changes, and then save the new version.

First we retrieve the document:

{% capture req %}

```json
GET /blog/1
```
{% endcapture %}
{% include curl.html req=req %}

The response body includes the same `_version` number of `1`:

```json
{
  "title": "My first blog entry",
  "text": "Just trying this out...",
  "_id": 1,
  "_version": 1
}
```

Now, when we try to save our changes by reindexing the document, we specify the
version to which our changes should be applied. We want this update to succeed
only if the current `_version` of this document in our index is version `1`, so
we pass `version=1` as a query param (or `_version` as part of the document body):

{% capture req %}

```json
PUT /blog/1?version=1

{
  "title": "My first blog entry",
  "text": "Starting to get the hang of this..."
}
```
{% endcapture %}
{% include curl.html req=req %}

This request succeeds, and the response body tells us that the `_version` has
been incremented to 2:

```json
{
  "title": "My first blog entry",
  "text": "Starting to get the hang of this...",
  "_id": 1,
  "_version": 2
}
```

However, if we were to rerun the same index request, still specifying
version `1`, Xapiand would respond with a `409 Conflict` HTTP response code.
