---
title: Query
---

The behaviour of a query clause depends on whether it is used in query:

## Queries

Queries within Xapiand are the mechanism by which documents are searched for within a database. They can be a simple search for text-based terms or a search based on the values assigned to documents, which can be combined using a number of different methods to produce more complex queries

## Simple Queries

The most basic query is a search for a single textual term. This will find all documents in the database which have the term banana assigned in the favoriteFruit field and restricting the size of results to 2 with the keyword `_limit` by default is set to 10:

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
	...
  "#query": {
    "#total_count": 2,
    "#matches_estimated": 17,
    "#hits": [
      {
        "city": "Kansas",
        "gender": "F",
        "balance": 3292.39,
        "firstname": "Joan",
        "lastname": "Howell",
        "employer": "Hawkster",
        "favoriteFruit": "strawberry",
        "eyeColor": "brown",
        "phone": "+1 (898) 456-2070",
        "state": "Indiana",
        "account_number": 720953,
        "address": "945 Driggs Avenue",
        "age": 30,
        "email": "joan.howell@hawkster.us",
        "_id": 59,
        "#docid": 68,
        "#rank": 0,
        "#weight": 4.186303264507511,
        "#percent": 100
      },
      {
        "city": "Kingstowne",
        "gender": "M",
        "balance": 2670.73,
        "firstname": "Debra",
        "lastname": "Callahan",
        "employer": "Dognost",
        "favoriteFruit": "banana",
        "eyeColor": "blue",
        "phone": "+1 (829) 409-2237",
        "state": "Indiana",
        "account_number": 448002,
        "address": "389 Waldorf Court",
        "age": 31,
        "email": "debra.callahan@dognost.biz",
        "_id": 146,
        "#docid": 134,
        "#rank": 1,
        "#weight": 4.186303264507511,
        "#percent": 100
      }
    ]
  },
  ...
}
```

When a query is executed, the result is a list of documents that match the query, together with a weight for each which indicates how good a match for the query that particular document is.

## Match All Query

The simple query, which matches all documents:

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

Each query produces a list of documents with a weight according to how good a match each document is for that query. These queries can then be combined to produce a more complex tree-like query structure, with the operators acting as branches within the tree

The most basic operators are the logical operators: `_or`, `_and` and `_not`

  * `_or`: Matches documents which are matched by either of the subqueries contained in the array
  * `_and`: Matches documents which are matched by all of the subqueries contained in the array
  * `_not`: match documents which don't match the subqueries.
  * `_xor`: matches documents which are matched by one or other of the subquerie, but not both (the expression is evaluated in pairs taken by the array)
  * `_and_maybe`: matches any document which matches the first element of the array and whether or not matches the rest


This example find all documents in which have the term banana in the favoriteFruit field or female (F) in gender field with brown in the eyecolor field and restricting the size of results to 2 with the keyword `_limit` by default is set to 10:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_or": [
      {
        "favoriteFruit": "banana"
      },
      {
        "_and": [
            { "gender": "F" }, { "eyeColor": "brown" }
        ]
      }
    ]
  },
  "_limit": 2
}
```
{% endcapture %}
{% include curl.html req=req %}

## Range Searches

The keyword `_range` matches documents where the given value is within the given fixed range (including both endpoints) This example find all documents in which have female (F) in gender field and with the age in the range of 20 to 40:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_and": [
      { "gender": "F" },
      {
        "age": {
          "_in": {
            "_range": {
              "_from": 20,
              "_to": 40
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

If you only use the keyword `_to` matches documents where the given value is less than or equal a fixed value.
If you only use the keyword `_from` matches documents where the given value is greater than or equal to a fixed value.
