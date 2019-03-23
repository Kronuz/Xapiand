---
title: Exploring Your Data
---

Now that we've gotten a glimpse of the basics, let's try to work on a more
realistic dataset. I've prepared a sample of fictitious JSON documents of
customer bank account information. Each document has the following form:

```json
{
  "accountNumber": 121931,
  "balance": 221.46,
  "employer": "Globoil",
  "name": {
    "firstName": "Michael",
    "lastName": "Lee"
  },
  "age": 24,
  "gender": "male",
  "contact": {
    "address": "630 Victor Road",
    "city": "Leyner",
    "state": "Indiana",
    "postcode": "61952",
    "phone": "+1 (924) 594-3216",
    "email": "michael.lee@globoil.co.uk"
  },
  "checkin": {
    "_point": {
      "_longitude": -95.63079,
      "_latitude": 31.76212
    }
  },
  "favoriteFruit": "lemon",
  "eyeColor": "blue",
  "style": {
    "_namespace": true,
    "_partial_paths": true,
    "clothing": {
      "pants": "khakis",
      "shirt": "t-shirt"
    },
    "hairstyle": "slick back"
  },
  "personality": {
    "_language": "en",
    "_value": "A lot can be assumed..."
  }
}
```

For the curious, this data was generated using [Faker](https://faker.readthedocs.io){:target="_blank"},
using the [generator.py]({{ '/assets/generator.py' | absolute_url }}){:target="_blank"}
script, so please ignore the actual values and semantics of the data as these
are all randomly generated.


## Loading the Sample Dataset

After downloading the [sample dataset]({{ '/assets/accounts.ndjson' | absolute_url }}){:target="_blank"},
let's load it into our cluster as follows:

{% capture req %}

```json
POST /bank/:restore
Content-Type: application/x-ndjson

@accounts.ndjson
```
{% endcapture %}
{% include curl.html req=req %}

{: .note .caution }
When using _curl_, make sure to use `--data-binary`, not `-d` or `--data`

More information about `:restore` can be found in the [Backups]({{ '/docs/reference-guide/backups' | relative_url }})
section.

After having loaded the dataset, you can then use the [Info API]({{ '/docs/reference-guide/info' | relative_url }})
to get information about the new index:

{% capture req %}

```json
GET /bank/:info
```
{% endcapture %}
{% include curl.html req=req %}

Response should be something like:

```json
{
  "database_info": {
    "endpoint": "bank",
    "doc_count": 1000,
    "last_id": 1000,
    "doc_del": 0,
    "av_length": 200.023,
    "doc_len_lower": 176,
    "doc_len_upper": 238,
    "has_positions": true,
    "shards": [
      "bank/.__1",
      "bank/.__2",
      "bank/.__3",
      "bank/.__4",
      "bank/.__5"
    ]
  }
}
```

Which means that we just successfully bulk indexed 1000 documents into the
bank index.


## The Search API

Now let's start with some simple searches. There are two basic ways to run
searches: one is by sending search parameters through the REST request URI and
the other by sending them through the REST request body. The request body
method allows you to be more expressive and also to define your searches in a
more readable JSON format. We'll try one example of the request URI method but
for the remainder of this guide, we will exclusively be using the request body
method.

The REST API for search is accessible from the `:search` endpoint. This example
returns all documents in the bank index:

{% capture req %}

```json
GET /bank/:search?q=*&sort=accountNumber
```
{% endcapture %}
{% include curl.html req=req %}

Let's first dissect the search call. We are searching (`:search` endpoint) in
the `bank` index, and the `q=*` parameter instructs Xapiand to _match all_
documents in the index (the default if not specified). The `sort=accountNumber`
parameter indicates to sort the results using the `accountNumber` field of each
document in an ascending order. The `pretty` parameter just tells Xapiand to
return pretty-printed JSON results, the same effect can be achieved by using the
`Accept` header as in: `Accept: application/json; indent: 2`.

And the response (partially shown):

```json
{
  "total": 1000,
  "count": 10,
  "hits": [
    {
      "accountNumber": 100123,
      "balance": 10073.05,
      "employer": "Affluex",
      "name": {
        "firstName": "Margaret",
        "lastName": "Anderson"
      },
      "age": 24,
      "gender": "female",
      "contact": {
        "address": "756 Strauss Street",
        "city": "Fairview",
        "state": "Virgin Islands",
        "postcode": "06099",
        "phone": "+1 (919) 400-3616",
        "email": "margaret.anderson@affluex.net"
      },
      "checkin": {
        "_point": {
          "_longitude": -122.39168,
          "_latitude": 40.58654
        }
      },
      "favoriteFruit": "lemon",
      "eyeColor": "brown",
      "style": {
        "clothing": {
          "pants": "mini-skirt",
          "shirt": "jersey",
          "footwear": "sneakers"
        }
      },
      "personality": "There's a lot to say about Margaret...",
      "_id": 233,
      "_version": 1,
      "#docid": 233,
      "#shard": 2,
      "#rank": 0,
      "#weight": 0.0,
      "#percent": 100
    }, ...
  ],
  "took": "39.889ms"
}
```

As for the response, we see the following parts:

* `total` - Number of estimated documents that match the query.
* `count` - Total number of returned hits.
* `hits`  - Search results.
* `took`  - Time it took to execute the query.

## Introducing the Query Language

Xapiand provides a JSON-style _domain-specific language_ that you can use to
execute queries. This is referred to as the [Query DSL](reference-guide/query-dsl).
The query language is quite comprehensive and can be intimidating at first
glance but the best way to actually learn it is to start with a few basic
examples.

{: .note .tip }
The **Query DSL** method for searching is much more efficient.

Going back to our last example, we executed a query to retrieve all documents
using `q=*`. Here is the same exact search using the alternative request body
method:

{% capture req %}

```json
POST /bank/:search

{
  "_query": "*",
  "_sort": "accountNumber"
}
```
{% endcapture %}
{% include curl.html req=req %}

The difference here is that instead of passing `q=*` in the URI, we `POST` a
JSON-style query request body to the `:search` API.

Dissecting the above, the query part tells us what our query definition is and
the `_query` part is simply the type of query that we want to run. The
`*` query is simply a search for all documents in the specified index.

In addition to the query parameter, we also can pass other parameters to
influence the search results. In the example in the section above we passed in
sort, here we pass in `limit`:

{% capture req %}

```json
POST /bank/:search

{
  "_query": "*",
  "_limit": 1
}
```
{% endcapture %}
{% include curl.html req=req %}

Note that if `limit` is not specified, it defaults to 10.

This example does a _match all_ and returns documents 10 through 19:

{% capture req %}

```json
POST /bank/:search

{
  "_query": "*",
  "_offset": 10,
  "_limit": 10
}
```
{% endcapture %}
{% include curl.html req=req %}

The `offset` parameter (0-based) specifies which document index to start from
and the `limit` parameter specifies how many documents to return starting at the
given `offset`. This feature is useful when implementing paging of search
results. Note that if `offset` is not specified, it defaults to 0.

This example does a _match all_ and sorts the results by account balance in
descending order and returns the top 10 (default for `limit`) documents.

{% capture req %}

```json
POST /bank/:search

{
  "_query": "*",
  "_sort": { "balance": { "_order": "desc" } }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Executing Searches

Now that we have seen a few of the basic search parameters, let's dig in some
more into the Query DSL. Let's first take a look at the returned document
fields. By default, the full JSON document is selected and returned as part of
all searches. If we don't want the entire document returned, we have the ability
to request only a few fields from within it to be returned by selecting them
by using `_selector` field during the search.

There are two types of selectors (which can be mixed):

+ Field Selector
+ Drill Selector

### Field Selector

It takes the form of `"{field1,field2}"`, and it selects only `field1` and
`field2` to be returned.

This example shows how to return two fields using the _Field Selector_,
`accountNumber` and `balance`, from the search:

{% capture req %}

```json
POST /bank/:search

{
  "_query": "*",
  "_selector": "{accountNumber,balance}"
}
```
{% endcapture %}
{% include curl.html req=req %}

### Drill Selector

It takes the form of `"field.sub_field.sub_sub_field"`, and it brings the
innermost field to the top level.

This example shows how to return a list of emails using the _Drill Selector_
from the search:

{% capture req %}

```json
POST /bank/:search

{
  "_query": "*",
  "_selector": "contact.email"
}
```
{% endcapture %}
{% include curl.html req=req %}


## Executing Filters

{% capture req %}

```json
POST /bank/:search

{
  "_query": {
    "balance": {
      "_in": {
        "_range": {
          "_from": 2000,
          "_to": 3000
        }
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Executing Aggregations

Aggregations provide the ability to group and extract statistics from your data.
The easiest way to think about aggregations is by roughly equating it to the
SQL `GROUP BY` and the SQL aggregate functions. In Xapiand, you have the ability
to execute searches returning hits and at the same time return aggregated
results separate from the hits all in one response. This is very powerful and
efficient in the sense that you can run queries and multiple aggregations and
get the results back of both (or either) operations in one shot avoiding network
roundtrips using a concise and simplified API.

To start with, this example groups all the accounts by state, and then returns
the count of accounts by state:

{% capture req %}

```json
GET /bank/:search

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggregations": {
    "group_by_state": {
      "_values": {
        "_field": "contact.state",
        "_keyed": true
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

In SQL, the above aggregation is similar in concept to:

```sql
SELECT state, COUNT(*) FROM bank GROUP BY state;
```

And the response (partially shown):

```json
{
  "total": 1000,
  "count": 0,
  "aggregations": {
    "_doc_count": 1000,
    "group_by_state": {
      "Texas": {
        "_doc_count": 15
      },
      "Pennsylvania": {
        "_doc_count": 20
      },
      "Oklahoma": {
        "_doc_count": 16
      },
      ...
      "Nebraska": {
        "_doc_count": 17
      }
    }
  },
  "hits": [],
  "took": "14.83ms"
}
```

There are many other aggregations capabilities that we won't go into detail here.
The [Aggregations Reference Guide]({{ '/docs/reference-guide/aggregations' | relative_url }})
is a great starting point if you want to do further experimentation.
