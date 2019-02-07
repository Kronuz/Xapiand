---
title: Query
---

The behaviour of a query clause depends on whether it is used in query:


## Queries

Queries within Xapiand are the mechanism by which documents are searched for
within a database. They can be a simple search for text-based terms or a search
based on the values assigned to documents, which can be combined using a number
of different methods to produce more complex queries.


## Simple Queries

The most basic query is a search for a single textual term. This will find all
documents in the database which have the term "_banana_" assigned in the
"_favoriteFruit_" field and restricting the size of results to 2 by using the
keyword `_limit`, by default is set to 10:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "favoriteFruit": "banana"
  },
  "_limit": 2
}
```
{% endcapture %}
{% include curl.html req=req %}


Resulting in:

```json
{
  "#query": {
    "#matches_estimated": 64,
    "#total_count": 2,
    "#hits": [
      {
        "accountNumber": 100481,
        "balance": 2234.1,
        "employer": "Homelux",
        "name": {
          "firstName": "Jamie",
          "lastName": "Obrien"
        },
        "age": 38,
        "gender": "female",
        "contact": {
          "address": "131 Pershing Loop",
          "city": "Orviston",
          "state": "Idaho",
          "postcode": "96728",
          "phone": "+1 (921) 490-2296",
          "email": "jamie.obrien@homelux.us"
        },
        "checkin": {
          "_point": {
            "_longitude": -82.90375,
            "_latitude": 32.54044
          }
        },
        "favoriteFruit": "banana",
        "eyeColor": "blue",
        "style": {
          "clothing": {
            "pants": "jeans",
            "shirt": "t-shirt",
            "footwear": "sneakers"
          },
          "hairstyle": "bald"
        },
        "personality": "Few know the true...",
        "_id": 234,
        "#docid": 206,
        "#rank": 0,
        "#weight": 2.729817989164937,
        "#percent": 100
      },
      {
        "accountNumber": 495974,
        "balance": 1085.45,
        "employer": "Pholio",
        "name": {
          "firstName": "Madison",
          "lastName": "Moore"
        },
        "age": 27,
        "gender": "female",
        "contact": {
          "address": "894 Windsor Place",
          "city": "Brandywine",
          "state": "Federated States Of Micronesia",
          "postcode": "06109",
          "phone": "+1 (885) 422-3158",
          "email": "madison.moore@pholio.net"
        },
        "checkin": {
          "_point": {
            "_longitude": -86.77917,
            "_latitude": 36.02506
          }
        },
        "favoriteFruit": "banana",
        "eyeColor": "blue",
        "style": {
          "clothing": {
            "pants": "skirt",
            "shirt": "lumberjack",
            "footwear": "high heels"
          },
          "hairstyle": "bob cut"
        },
        "personality": "Few know the true...",
        "_id": 830,
        "#docid": 833,
        "#rank": 1,
        "#weight": 2.7197230217708419,
        "#percent": 99
      }
    ]
  }
}
```

When a query is executed, the result is a list of documents that match the
query, together with a **weight**, a **rank** and a **percent** for each which
indicates how good a match for the query that particular document is.


## Match All Query

The simplest query, which matches all documents:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": "*"
}
```
{% endcapture %}
{% include curl.html req=req %}


## Filtering and Logical Operators

Each query produces a list of documents with a weight according to how good a
match each document is for that query. These queries can then be combined to
produce a more complex tree-like query structure, with the operators acting as
branches within the tree.

The most basic operators are the logical operators: `_or`, `_and` and `_not`,
but there are other more advanced operators:


* `_and`          - Finds documents which are matched by all of the subqueries.
* `_or`           - Finds documents which are matched by either of the subqueries.
* `_not`          - Finds documents which don't match any of the subqueries.
* `_and_not`      - 
* `_xor`          - Finds documents which are matched by one or other of the subquerie,
                    but not both (the expression is evaluated in pairs taken by the array)
* `_and_maybe`    - Finds any document which matches the first element of the array
                    and whether or not matches the rest.
* `_filter`       - 
* `_near`         - 
* `_phrase`       - 
* `_value_range`  - 
* `_scale_weight` - 
* `_elite_set`    - 
* `_value_ge`     - 
* `_value_le`     - 
* `_synonym`      - 
* `_max`          - 
* `_wildcard`     - 

The following example finds _all_ bank accounts for which their account
holders are either _brown_ eyed _females_ or like _bananas_:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_or": [
      {
        "_and": [
            { "gender": "female" },
            { "eyeColor": "brown" }
        ]
      },
      {
        "favoriteFruit": "banana"
      }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Range Searches

The keyword `_range` matches documents where the given value is within the given
fixed range (including both endpoints).

* If you only use the keyword `_to` matches documents where the given value is
  less than or equal a fixed value.

* If you only use the keyword `_from` matches documents where the given value is
  greater than or equal to a fixed value.

This example find _all_ bank accounts for which their account holders are
_females_ in the ages between 20 and 30:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_and": [
      { "gender": "female" },
      {
        "age": {
          "_in": {
            "_range": {
              "_from": 20,
              "_to": 30
            }
          }
        }
      }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
