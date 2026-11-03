---
title: "API Conventions"
---

---

## RESTful Features

Xapiand uses a RESTful API exposed using JSON over HTTP.

When we talk about our API, we use terms like "_REST_" and "_RESTful_." "_REST_"
stands for [Representational State Transfer](https://en.wikipedia.org/wiki/Representational_state_transfer).

The conventions listed in here can be applied throughout the REST API, unless
otherwise specified.

### RESTful HTTP Methods

You may see these standard HTTP methods referred to as CRUD, or _Create_, _Read_,
_Update_, _Delete_. Although CRUD has roots in database operations, you can also
map those operations to the standard HTTP methods. For example, use a `POST`
request to create a new resource, a `GET` request to read or retrieve a resource,
a `PATCH` or `UPDATE` request to edit a resource, and a `DELETE` request to
delete a resource.

### Deviations from REST

We do our best to use standard HTTP methods with accurate and well-known status
codes in the Xapiand API, but here are some additions and deviations.

Additionally to the standard HTTP methods, we also use other custom methods
such as `UPDATE`, for certain operations.

---

## HTTP Methods

- **_Safe Methods_**<br>
  Requests that use _safe_ HTTP methods won't alter a resource at all. Examples
  are `GET`, `OPTIONS` and `HEAD`.

- **_Idempotent Methods_**<br>
  An _idempotent_ HTTP method is a HTTP method that can be called many times
  without different outcomes. It would not matter if the method is called only
  once, twice or a hundred times over, the result should be the same. This only
  applies to the result, not the resource itself. Examples are `DELETE`, `PUT`.

- **_Not Safe/Idempotent Methods_**<br>
  This are requests that will alter the resource *and* potentially end up with
  different results every time. Examples are `POST`.

### Custom Methods

The _Standard Methods_, the ones we all are familiar with, have simpler and
well-defined semantics but there is functionality that cannot be easily
expressed via standard methods. Custom methods refer to such API methods.

Simply pass the required method as a non-standard HTTP method in the request.

Example:

```rest
INFO /some/resource/name
```

<!-- e2e:begin
---
status: 404
---
e2e:end -->

If your firewall rules don't support non-standard HTTP methods like `PATCH`,
`UPDATE` or `DELETE`, for example, you have two options:

+ Use HTTP Method Override
+ Use HTTP Method Mapping

#### HTTP Method Override

You can use the [X-HTTP-Method-Override](http://www.hanselman.com/blog/HTTPPUTOrDELETENotAllowedUseXHTTPMethodOverrideForYourRESTServiceWithASPNETWebAPI.aspx)
(or _HTTP-Method-Override_) header. Pass the method you want to use in the
`X-HTTP-Method-Override` header and use the `POST` method.

Example:

```rest
POST /some/resource/name
X-HTTP-Method-Override: INFO
```

<!-- e2e:begin
---
status: 404
---
e2e:end -->

:::hint{.caution}
The Method Override won't work with any other method other than `POST`, you'll
receive an error.
:::

#### HTTP Method Mapping

Custom methods can use the following generic HTTP mapping:
`http://service.name:8880/some/resource/name:customMethod`

To use method mappings pass the mapping in the URL and use HTTP `POST` verb
since it has the most flexible semantics, except for methods serving as an
alternative get or list which may use `GET`.

Example:

```rest
GET /some/resource/name:info
```

<!-- e2e:begin
---
status: 404
---
e2e:end -->

---

## Resource Paths

### To slash or not to slash

That is the question we hear often. Onward to the answers! Historically,
it's common for URLs _with a trailing slash_ to indicate a _directory_, and
those _without a trailing slash_ to denote a _file_:

+ `http://example.com/foo` (without trailing slash, conventionally a file)
+ `http://example.com/foo/` (with trailing slash, conventionally a directory)

Source: [Google WebMaster Central Blog - To slash or not to slash](https://webmasters.googleblog.com/2010/04/to-slash-or-not-to-slash.html)

### Trailing slashes are important

To us, trailing slashes are important to distinguish between a path to an
_Index_ (a _directory_) and a path to a _Document_ (a _file_).

The following will delete a _single document_ from index `/some/resource/name`,
the document with ID `name`:

```rest
DELETE /some/resource/name
```

<!-- e2e:begin
---
status: 404
---
e2e:end -->

Whilst the next example will delete the whole index `/some/resource/path/` with
_all its documents_ in it as well:

```rest
DELETE /some/resource/path/
```

<!-- e2e:begin
---
status: 501
---
e2e:end -->

:::hint[Remember]{.warning}
Trailing slashes in resource paths are important, always make sure you are
requesting a method for the proper resource path.
**Trailing slashes _do mean something_**.
:::

---

## The Node

The root of the API is the node itself. It names no index, so it has no documents:
`GET /` answers with the node's name, its server and library versions, its options
and its state, and `HEAD /` answers `200` when the node is up.

```rest
GET /
```

<!-- e2e:begin

```js
pm.test("The root describes the node", function() {
  pm.response.to.have.status(200);
  var jsonData = pm.response.json();
  pm.expect(jsonData["server"]["name"]).to.equal("Xapiand");
  pm.expect(jsonData["versions"]["Xapiand"]).to.be.a("string");
  pm.expect(jsonData["options"]["thread_pools"]["num_shards"]).to.be.a("number");
  pm.expect(jsonData["state"]).to.be.a("string");
});
```
e2e:end -->

A [field selector](#field-expansion) reduces the answer, `GET /.state` answers just the node's
state, and `GET /:info` answers the same as `GET /`. `GET /:metrics` answers the
node's metrics in the Prometheus text format. A request that needs an index (`PUT /`,
`POST /`, `DELETE /`, a search) and names none answers `400`.

```rest
GET /.server.name
```

<!-- e2e:begin

```js
pm.test("A selector reduces the node's answer", function() {
  pm.response.to.have.status(200);
  pm.expect(pm.response.json()).to.equal("Xapiand");
});
```
e2e:end -->

```rest
PUT /
```

<!-- e2e:begin
---
status: 400
---
e2e:end -->

---

## JSON, MessagePack, and YAML

The Xapiand API can process JSON, MessagePack, or YAML objects. Choose the format
with the request's `Content-Type` header (`application/json`,
`application/x-msgpack`, or `application/x-yaml`); JSON is the default when no
`Content-Type` is given.

:::hint[MessagePack]{.tip}
[MessagePack](https://msgpack.org) is more efficient and it
is our internal representation of the data.
:::

### YAML

You can also send documents as [YAML](https://yaml.org) by setting the
`Content-Type` to `application/x-yaml`. Xapiand parses the body into the same
internal object it would from the equivalent JSON:

```rest
PUT /twitter/user/Jane?commit
Content-Type: application/x-yaml

name: Jane
age: 30
city: Turin
```

```rest
GET /twitter/user/Jane
```

<!-- e2e:begin
```js
pm.test("YAML document was indexed", function () {
  pm.expect(pm.response.json().name).to.eql("Jane");
  pm.expect(pm.response.json().city).to.eql("Turin");
});
```
e2e:end -->

### Deviations from JSON

#### Comments

JSON can have C-style `/* */` block or single line `//` comments. Comments are
allowed everywhere in the JSON document.

#### Trailing Commas

JSON can have trailing commas.

---

## Field Expansion

JSON or MessagePack fields in objects passed to Xapiand are expanded.
For example, the following nested object:

```json
{
  "contact": {
    "address": {
      "country": {
        "name": "Italy"
      }
    }
  }
}
```

Is equivalent to:

```json
{
  "contact.address.country.name": "Italy"
}
```

---

## Multiple Indexes

Most APIs that refer to an index parameter support execution across multiple
indexes, using simple `test1,test2,test3` notation.

Searches and counts also take `_all` for all the indexes of the node, and globs
over the segments of an index path (the parts between `/`), for example
`test*`, `*test`, `te*t`, `test-??`, `test-[0-9]*` or `logs/**/error-*`, and `-`
to exclude, for example `test*,-test3`:

- A `*` stands for any run of characters inside one segment, none included. It
  never matches a `/`: `test*` matches `test` and `test1`, and not `test/old`
  (that is `test*/*`).
- A `?` stands for one character, in one segment: `test-??` matches `test-ab`.
- A `[...]` stands for one character of a class: `[abc]`, a range `[a-f]`, or
  every character but those with a leading `!` or `^`: `[!0-9]`. A `[` with no
  closing `]` is a plain `[`.
- A segment that is `**` stands for any number of segments, none included, and
  may be used more than once: `**/some-??` matches `some-ab` at any depth, and
  `logs/**/error-*/**/last` matches `logs/error-1/last`, `logs/a/error-1/b/c/last`
  and so on. `logs/**` matches `logs` and everything below it.
- `_all` is the same as `**`.
- An item that starts with `-` removes the indexes it matches from the result,
  wherever it stands in the list.
- A glob never matches an index whose name starts with a `.` (the node's own
  indexes); name it to search it.
- A glob that matches no index finds nothing: the search answers `200` with
  no hits.
- A search over more than `--max-search-indexes` indexes (100,000 by default),
  counting the ones a glob expands to, answers `400`; so does one whose
  patterns read more than that many directory entries to find them, in the
  directories of an index too. The search goes only into the directories a
  pattern can still match below: `logs/2026/*` reads `logs` and `logs/2026`,
  and no other directory. A search names at most 32 patterns (globs and
  exclusions together), and an item repeated counts once. A wide search reads
  its indexes in batches; see
  [Searching many indexes](/Xapiand/options#searching-many-indexes).

A `*` used to match a `/` as well (`test*` matched `test1/old`); to reach the
indexes below a name, give the segments: `test*/*`, or `test*/**`.

Globs match the indexes that are on the node's disk. They are the way to reach
a great many indexes: a URL holds 64 KiB, which is a few thousand index names.

Indexes nest: `twitter` and `twitter/user` can both exist, the second inside the
first. `twitter*` finds the first, `twitter/*` the second, and `twitter/**` or
`_all` both. In a URL a `?` starts the query string; write it `%3F` in an index
pattern.

A wildcard or `_all` reads every index on the node, so a name that is not given
no longer keeps an index out of a search: a proxy that allows a request by the
first segment of its path has to treat `,`, `*`, `_all` and a leading `-` as
naming several indexes, and `:count` on a prefix tells whether names exist.

Four indexes, one holding two documents:

```rest
PUT /multi/alpha/one

{
  "marker": "multiindex",
  "name": "alpha one"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /multi/beta/one

{
  "marker": "multiindex",
  "name": "beta one"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /multi/gamma/one

{
  "marker": "multiindex",
  "name": "gamma one"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /multi/gamma/two

{
  "marker": "multiindex",
  "name": "gamma two"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /multi/old/delta/one

{
  "marker": "multiindex",
  "name": "delta one"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

Search two of them by listing them:

```rest
SEARCH /multi/alpha,multi/beta/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("Two indexes, two documents", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(2);
  pm.expect(jsonData.hits.length).to.equal(2);
});
```
e2e:end -->

Search the indexes directly under `multi/` with a `*`, which stays in one segment:

```rest
SEARCH /multi/*/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("The star reaches the three indexes one level down", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(4);
  var names = jsonData.hits.map(function(hit) { return hit.name; }).sort();
  pm.expect(names).to.eql(["alpha one", "beta one", "gamma one", "gamma two"]);
});
```
e2e:end -->

Every index at any depth under `multi/` with `**`:

```rest
SEARCH /multi/**/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("The double star reaches all four indexes", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(5);
  var names = jsonData.hits.map(function(hit) { return hit.name; }).sort();
  pm.expect(names).to.eql(["alpha one", "beta one", "delta one", "gamma one", "gamma two"]);
});
```
e2e:end -->

An index at any depth by its last name:

```rest
SEARCH /**/delta/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("Only delta, two levels down", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  pm.expect(jsonData.hits[0].name).to.equal("delta one");
});
```
e2e:end -->

A `?` stands for one character, and a class for one of a set (a `?` in a URL starts the query, so it is written `%3F`):

```rest
SEARCH /multi/alph%3F,multi/[bg]*/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("alpha, beta and gamma", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(4);
  pm.expect(jsonData.hits.map(function(hit) { return hit.name; }).sort()).to.eql(["alpha one", "beta one", "gamma one", "gamma two"]);
});
```
e2e:end -->

A wildcard inside a name:

```rest
SEARCH /multi/g*/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("Only gamma", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(2);
  pm.expect(jsonData.hits.map(function(hit) { return hit.name; }).sort()).to.eql(["gamma one", "gamma two"]);
});
```
e2e:end -->

Take indexes out with `-`:

```rest
SEARCH /multi/**,-multi/g*,-multi/old/*/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("Everything but gamma and the old ones", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(2);
  pm.expect(jsonData.hits.map(function(hit) { return hit.name; }).sort()).to.eql(["alpha one", "beta one"]);
});
```
e2e:end -->

`_all` searches every index of the node:

```rest
SEARCH /_all/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("_all reaches the same five documents", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(5);
});
```
e2e:end -->

A count takes the same patterns:

```rest
COUNT /multi/**/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("The count over the wildcard", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(5);
});
```
e2e:end -->

A wildcard that matches nothing finds nothing:

```rest
SEARCH /multi/nomatch*/?q=marker:multiindex
```

<!-- e2e:begin
```js
pm.test("No index matches", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(0);
  pm.expect(jsonData.hits.length).to.equal(0);
});
```
e2e:end -->

Indexes nest: an index may hold others, and a glob finds each at its own depth. Make `parent` and `parent/child`:

```rest
PUT /parent/doc

{
  "marker": "nestedindex",
  "name": "parent doc"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

```rest
PUT /parent/child/doc

{
  "marker": "nestedindex",
  "name": "child doc"
}
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```
e2e:end -->

`parent*` is one segment, so it finds the parent only:

```rest
SEARCH /parent*/?q=marker:nestedindex
```

<!-- e2e:begin
```js
pm.test("The parent only", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  pm.expect(jsonData.hits[0].name).to.equal("parent doc");
});
```
e2e:end -->

`parent/*` is the indexes in it, and `parent/**` the parent and everything in it:

```rest
SEARCH /parent/*/?q=marker:nestedindex
```

<!-- e2e:begin
```js
pm.test("The child only", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(1);
  pm.expect(jsonData.hits[0].name).to.equal("child doc");
});
```
e2e:end -->

```rest
SEARCH /parent/**/?q=marker:nestedindex
```

<!-- e2e:begin
```js
pm.test("Both, the parent and the index in it", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(2);
  pm.expect(jsonData.hits.map(function(hit) { return hit.name; }).sort()).to.eql(["child doc", "parent doc"]);
});
```
e2e:end -->

`_all` is `**`: it finds both too.

```rest
SEARCH /_all/?q=marker:nestedindex
```

<!-- e2e:begin
```js
pm.test("Both, the parent and the index in it", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(2);
  pm.expect(jsonData.hits.map(function(hit) { return hit.name; }).sort()).to.eql(["child doc", "parent doc"]);
});
```
e2e:end -->

```rest
COUNT /parent*,parent/*/?q=marker:nestedindex
```

<!-- e2e:begin
```js
pm.test("Two names, two indexes", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(2);
});
```
e2e:end -->

:::hint{.info}
Single index APIs such as the Document APIs and the single-index alias APIs
do not support multiple indexes.
:::
