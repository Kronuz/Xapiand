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
The override header is read on a `POST` (or `GET`) request. Any other method
carrying it, such as `PUT`, answers `400`.
:::

#### HTTP Method Mapping

Custom methods can use the following generic HTTP mapping:
`http://service.name:8880/some/resource/name:customMethod`

To use method mappings pass the mapping in the URL and use HTTP `POST` verb
since it has the most flexible semantics, except for methods serving as an
alternative get or list which may use `GET`. Any other method answers `400`
("HTTP Mappings must use GET or POST method").

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

Whilst the next one addresses the _index_ `/some/resource/path/`. Xapiand does not
delete indexes over HTTP: `DELETE` on an index path answers `501 Not Implemented`
(see [Delete Index](/Xapiand/reference-guide/indexes/delete-api)):

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

## The Verbs

A request is a verb and a path. The path names a _document_ (it ends with an ID), an
_index_ (it ends with a `/`), a command (`/index/:command`) or the node (`/`). A verb
the path does not accept answers `405 Method Not Allowed`.

| Verb | On | What it does |
|---|---|---|
| `GET` | node, index, document | The node, the index's settings and schema, a [document](/Xapiand/reference-guide/documents/get-api); `GET /:metrics` answers the metrics |
| `HEAD` | index, document | `200` if it exists, `404` if not |
| `SEARCH`, `COUNT` | one or many indexes | [Search](/Xapiand/reference-guide/search/search-api), or count the hits |
| `INFO` | one index, document | Statistics of an [index](/Xapiand/reference-guide/indexes/info-api) or [document](/Xapiand/reference-guide/documents/info-api) |
| `PUT` | index, document | Create an [index](/Xapiand/reference-guide/indexes/create-api) or [index a document](/Xapiand/reference-guide/documents/index-api) |
| `POST` | index | Index a document with a generated ID (`201` and `Location`) |
| `UPDATE`, `PATCH`, `UPSERT` | index, document | [Update](/Xapiand/reference-guide/documents/update-api) a document; `UPSERT` creates it when missing |
| `DELETE` | document | [Delete](/Xapiand/reference-guide/documents/delete-api) a document. On an index it answers `501`, on `/` it answers `405` |
| `COMMIT` | one index | Commit the index now (`200`) |
| `CHECK` | one index | Check the index's files; answers `{"errors": {<index>: <count>}}` |
| `DUMP`, `RESTORE` | one index, document | [Dump or restore](/Xapiand/reference-guide/indexes/backups) its documents |
| `WAL` | one single-shard index | The [write-ahead log](/Xapiand/reference-guide/wal) |
| `FLUSH`, `QUIT` | `/` | [Administrative commands](#administrative-commands), only with `--admin-commands` |
| `OPTIONS` | any | `200` with an `Allow` header |
| `OPEN`, `CLOSE` | any | `429 Too Many Requests` | Too many searches wait to look up the index names of their patterns (`--search-walk-queue`); `Retry-After` says when to try again. |
| `501 Not Implemented` |
| `LOCK`, `UNLOCK`, `COPY`, `MOVE`, `PURGE`, `LINK`, `UNLINK`, `TRACE`, `CONNECT` | any | `405` |

Sources: the verb list and the route table, `src/server/search_views.cc:187-218` and
`548-602`; the inline answers, `:652-672`.

```rest
OPTIONS /twitter/user/
```

<!-- e2e:begin
---
description: OPTIONS answers with the allowed methods
---

```js
pm.test("OPTIONS lists the methods", function() {
  pm.response.to.have.status(200);
  pm.expect(pm.response.headers.get("Allow")).to.include("GET");
});
```
e2e:end -->

A verb that is not served at that path answers `405`:

```rest
LOCK /twitter/user/
```

<!-- e2e:begin
---
status: 405
---
e2e:end -->

```rest
COMMIT /twitter/user/John
```

<!-- e2e:begin
---
description: COMMIT takes an index, not a document
status: 405
---
e2e:end -->

`COMMIT` commits an index now instead of waiting for the autocommit (see
[Visibility and Durability](/Xapiand/basics#visibility-and-durability)):

```rest
COMMIT /twitter/user/
```

<!-- e2e:begin
---
description: COMMIT an index
---

```js
pm.test("COMMIT answers 200", function() {
  pm.response.to.have.status(200);
});
```
e2e:end -->

`CHECK` checks an index and answers how many errors it found in each shard:

```rest
CHECK /twitter/user/
```

<!-- e2e:begin
---
description: CHECK an index
---

```js
pm.test("CHECK finds no errors", function() {
  pm.response.to.have.status(200);
  var errors = pm.response.json().errors;
  Object.keys(errors).forEach(function(shard) {
    pm.expect(errors[shard]).to.equal(0);
  });
});
```
e2e:end -->

## Administrative Commands

`FLUSH` and `QUIT` are answered only at the root URL, and only when the node runs
with `--admin-commands`; otherwise (and on any other path) they answer `405`.

- `FLUSH` commits and closes every open index and closes the client connections, and
  answers `200` when they are closed. `FLUSH /?databases` flushes only the indexes,
  `FLUSH /?clients` only the connections.
- `QUIT` asks the node to shut down, answers `200` and closes the connection (see
  [Stopping the node](/Xapiand/options#stopping-the-node)).

The API has no authentication: enable the option only on a node that untrusted clients
cannot reach (see [Options](/Xapiand/options#administrative-commands)). Sources:
`src/server/search_views.cc:1497-1535` (`flush_view`, `quit_view`).

```rest
QUIT /
```

<!-- e2e:begin
---
description: QUIT needs --admin-commands
---

```js
pm.test("QUIT is not allowed without --admin-commands", function() {
  pm.response.to.have.status(405);
});
```
e2e:end -->

```rest
FLUSH /twitter/
```

<!-- e2e:begin
---
description: FLUSH answers only at the root
---

```js
pm.test("FLUSH on an index path is not allowed", function() {
  pm.response.to.have.status(405);
});
```
e2e:end -->

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
`POST /`, a search or a count) and names none answers `400`; `DELETE /` answers `405`.

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

`SEARCH` and `COUNT` run across multiple indexes, using simple
`test1,test2,test3` notation. Every other verb takes a single index (a list
answers `400`, "Method can only be used with single indexes").

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

To reach the indexes below a name, give the segments: `test*/*`, or `test*/**`.

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
Single index APIs such as the Document APIs, `INFO`, `COMMIT`, `CHECK`, `DUMP`,
`RESTORE` and `WAL` do not support multiple indexes.
:::

---

## Statuses

A failed request answers a JSON body with the `status`, its `type` and a `message`
(`{"status": 400, "type": "Bad Request", "message": "Bad Request: ..."}`).

| Status | Meaning |
|---|---|
| `200 OK` | The request succeeded and has a body. |
| `201 Created` | `POST` without an ID: the new document's URL is in `Location`. |
| `204 No Content` | A write succeeded and echoes nothing (the default; see [Quick-start](/Xapiand/quickstart#what-a-write-answers)). |
| `400 Bad Request` | The request is malformed or past a limit; the `message` says which. |
| `404 Not Found` | The document or index is not there. |
| `405 Method Not Allowed` | The verb is not served at that path. |
| `406 Not Acceptable` | No format in `Accept` can be served. |
| `409 Conflict` | A write's `?version` does not match the document's version; the body's `_version` is the current one (`0`: no document). |
| `412 Precondition Failed` | A strict index refuses a field or setting that has no type. |
| `413 Payload Too Large` | The body is past the limit of the request. |
| `501 Not Implemented` | The verb is known and not implemented here (`OPEN`, `CLOSE`, `DELETE` on an index). |
| `503 Service Unavailable` | The index is busy or the node holds too many requests; `Retry-After` says when to try again. |

Sources: `src/server/http_utils.h:48-135` (the mapping), `src/server/search_views.cc:3182`
(`413`), `src/manager.cc:950,961` and the http library's `http_asio.h:1062-1066` (`503`).

## Limits

Each limit answers `400` unless it says otherwise, and each is a constant of the
code. The table repeats the
[architecture page's](/Xapiand/architecture#limits) with the verbs they apply to.

| What | Limit | Source |
|---|---|---|
| A buffered body | 16 MiB (`413`) | `src/server/search_service.h:50` |
| Nesting in a JSON, YAML or MessagePack body | 200 levels | `src/database/utils.h:143` |
| A query | 100 levels, 4,096 clauses, 256 phrase or near groups joined by one AND | `src/engine/query.h:94-98` |
| Nesting of aggregations | 16 levels | `src/aggregations/aggregations.h:40` |
| Live buckets in one search | 65,535 | `src/aggregations/aggregations.h:39` |
| Index patterns in one search | 32 | `src/index_pattern.h:126` |
| Indexes one search reads | `--max-search-indexes` (100,000) | `src/opts.cc` |
| A Lua script call | 10^8 instructions, 64 MiB of memory | `src/lua/processor.h:50-62` |

Past 32 index patterns a search answers `400`:

```rest
SEARCH /p0*,p1*,p2*,p3*,p4*,p5*,p6*,p7*,p8*,p9*,p10*,p11*,p12*,p13*,p14*,p15*,p16*,p17*,p18*,p19*,p20*,p21*,p22*,p23*,p24*,p25*,p26*,p27*,p28*,p29*,p30*,p31*,p32*/?q=x
```

<!-- e2e:begin
---
description: Limits: 33 index patterns
---

```js
pm.test("A search names at most 32 patterns", function() {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("32 index patterns");
});
```
e2e:end -->

An aggregation nested 17 levels deep (the body is on one line) is refused:

```rest
SEARCH /bank/?limit=0

{"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
```

<!-- e2e:begin
---
description: Limits: aggregations nested 17 levels
---

```js
pm.test("Aggregations nest at most 16 levels", function() {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("16 levels");
});
```
e2e:end -->

The same aggregation at 16 levels runs:

```rest
SEARCH /bank/?limit=0

{"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"},"_aggs":{"a":{"_values":{"_field":"gender"}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}}
```

<!-- e2e:begin
---
description: Limits: aggregations nested 16 levels
---

```js
pm.test("Sixteen levels run", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().aggregations).to.have.property("a");
});
```
e2e:end -->

A body that nests arrays or objects 201 levels deep is refused before it is read:

```rest
PUT /limits/deep

[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
```

<!-- e2e:begin
---
description: Limits: a body nested 201 levels
---

```js
pm.test("A body nests at most 200 levels", function() {
  pm.response.to.have.status(400);
  pm.expect(pm.response.json().message).to.include("200 levels");
});
```
e2e:end -->
