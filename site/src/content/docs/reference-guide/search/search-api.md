---
title: "Search API"
---

You can search for documents by using the `SEARCH` method:

```rest
SEARCH /bank/
```

<!-- e2e:begin

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Search API results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.hits.length).to.equal(10);
});
```
e2e:end -->

:::hint{.tip}
It is also possible to use [HTTP Method Mappings](/Xapiand/reference-guide/api#http-method-mapping).
:::

## The answer

A search answers `200` with the hits, how many came back and how many match:

```json
{
  "hits": [ { "accountNumber": 121931, "age": 24, "_id": 1, "_version": 1 } ],
  "count": 1,
  "total": 1000,
  "took": 0.0012
}
```

- `hits` are the documents, each with its `_id` and `_version`. With `comments=true` each
  also carries `#docid`, `#shard`, `#rank`, `#weight` and `#percent`.
- `count` is the number of hits in the answer, and `total` is the number of documents that
  match (after a [collapse](#collapsing-results), the hits that are left).
- `took` is the search's time in seconds (`human=true` writes it as a string such as `1.2 ms`).
- `aggregations` is there when the body asks for [aggregations](/Xapiand/reference-guide/search/aggregations).

A search over an index that does not exist, or a pattern that matches none, is not an
error: it answers `200` with no hits.

## Parameters

The query string and the body of a `SEARCH` (or `COUNT`) are both read. The body keys
start with an underscore. A body `_query`, `_limit`, `_offset` and `_check_at_least`
replace the parameters of the same name; a body `_sort` and `sort` parameters add up.

| Parameter | Body key | Meaning |
|---|---|---|
| `q`, `query` | `_query` | The query. A parameter is a [query string](/Xapiand/reference-guide/search/query-dsl/leaf-queries/text-queries#the-query-string-grammar) and may be repeated: the queries are ANDed. The body takes the [Query DSL](/Xapiand/reference-guide/search/query-dsl). With neither, every document matches. |
| `limit` | `_limit` | The most hits to return (10). |
| `offset` | `_offset` | The hits to skip (0). |
| `sort` | `_sort` | The order of the hits; the parameter may be repeated. |
| `check_at_least` | `_check_at_least` | The least documents to examine; the node reads every match when the search has aggregations. |
| `collapse`, `collapse_max` | | See [Collapsing results](#collapsing-results). |
| `selector` | `_selector` | Keeps one part of each hit (`accountNumber`, `contact.city`). |
| `volatile` | | Opens the index for writing, so the search sees the writer's latest view. |
| `pretty`, `human`, `comments` | | How the answer is written; see [Options](/Xapiand/options). |
| `spelling`, `synonyms`, `icase` | | Read and ignored. |

Sources: `src/server/search_views.cc:2340-2470` (the parameters), `src/database/handler.cc:1140-1230`
(the body keys), `src/server/search_views.cc:1932-2090` (the answer).

`limit` and `offset` page through the hits:

```rest
SEARCH /bank/?limit=3&offset=2&sort=-accountNumber
```

<!-- e2e:begin
---
description: Search parameters: limit, offset and sort
---

```js
pm.test("Three hits, sorted by accountNumber descending", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(3);
  pm.expect(jsonData.total).to.equal(1000);
  pm.expect(jsonData.hits.length).to.equal(3);
  var numbers = jsonData.hits.map(function(hit) { return hit.accountNumber; });
  pm.expect(numbers[0]).to.be.above(numbers[1]);
  pm.expect(numbers[1]).to.be.above(numbers[2]);
});
```
e2e:end -->

`sort` takes a field (ascending), a field with a leading `-` (descending), and may be given
more than once, the later ones breaking ties. A body `_sort` takes a field, a list of
fields, or an object with `_order`: `{"balance": {"_order": "desc"}}`
([Exploration](/Xapiand/exploration)).

```rest
SEARCH /bank/?limit=3&sort=age&sort=-accountNumber
```

<!-- e2e:begin
---
description: Search parameters: two sort keys
---

```js
pm.test("Sorted by age, then by account number descending", function() {
  pm.response.to.be.success;
  var hits = pm.response.json().hits;
  pm.expect(hits.length).to.equal(3);
  pm.expect(hits[0].age).to.be.at.most(hits[1].age);
  if (hits[0].age === hits[1].age) {
    pm.expect(hits[0].accountNumber).to.be.above(hits[1].accountNumber);
  }
});
```
e2e:end -->

A repeated `q` ANDs its queries:

```rest
SEARCH /bank/?q=age:30&q=eyeColor:blue&limit=2
```

<!-- e2e:begin
---
description: Search parameters: two q
---

```js
pm.test("Both queries must match", function() {
  pm.response.to.be.success;
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(2);
  pm.expect(jsonData.total).to.be.below(56);
  jsonData.hits.forEach(function(hit) {
    pm.expect(hit.age).to.equal(30);
    pm.expect(hit.eyeColor).to.equal("blue");
  });
});
```
e2e:end -->

`limit=0` asks for no hits, only the total, and `selector` keeps one part of each hit:

```rest
SEARCH /bank/?q=age:30&limit=2&selector=accountNumber
```

<!-- e2e:begin
---
description: Search parameters: selector
---

```js
pm.test("The hits are the account numbers", function() {
  pm.response.to.be.success;
  var hits = pm.response.json().hits;
  pm.expect(hits.length).to.equal(2);
  hits.forEach(function(hit) { pm.expect(hit).to.be.a("number"); });
});
```
e2e:end -->

`COUNT` takes the same query and answers only the total:

```rest
COUNT /bank/?q=age:30
```

<!-- e2e:begin
---
description: Search parameters: COUNT
---

```js
pm.test("COUNT answers the total", function() {
  pm.response.to.be.success;
  pm.expect(pm.response.json().total).to.equal(56);
});
```
e2e:end -->

## Collapsing results

`collapse` keeps the best hit for each distinct value of a field, and drops the rest. Documents
without a value for the field are never collapsed. Here the account holders are collapsed by eye color,
so one account of each color comes back:

```rest
SEARCH /bank/?collapse=eyeColor
```

<!-- e2e:begin
```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("One hit per eye color", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.total).to.equal(3);
  pm.expect(jsonData.hits.length).to.equal(3);
  var colors = jsonData.hits.map(function(hit) { return hit.eyeColor; });
  pm.expect(colors).to.have.members(['blue', 'brown', 'green']);
});
```
e2e:end -->

The hits keep their order (by relevance, or by `sort`), `total` counts the hits that are left, and
`limit` and `offset` apply to them. `collapse_max` keeps more than one hit per value (it is 1 by default).
