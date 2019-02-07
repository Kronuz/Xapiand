---
title: Queries
---

Queries within Xapiand are the mechanism by which documents are searched for
within a database. They can be a simple search for text-based terms or a search
based on the values assigned to documents, which can be combined using a number
of different methods to produce more complex queries.


## Simple Queries

The most basic query is a search for a single textual term. This will find all
documents in the database which have that term assigned to them. These kind of
queries are called "**leaf queries**".

#### Example

For example, a search might be for the term "_banana_" assigned in the
"_favoriteFruit_" field and restricting the size of results to one result by
using the keyword `_limit`, which by default is set to 10:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "favoriteFruit": "banana"
  },
  "_limit": 1
}
```
{% endcapture %}
{% include curl.html req=req %}

When a query is executed, the result is a list of documents that match the
query, together with a **weight**, a **rank** and a **percent** for each which
indicates how good a match for the query that particular document is.


## Logical Operators

Each query produces a list of documents with a weight according to how good a
match each document is for that query. These queries can then be combined to
produce a more complex tree-like query structure, with the operators acting as
branches within the tree.

The most basic operators are the logical operators: `_or`, `_and` and `_not` -
these match documents in the following way:

* `_or`           - Finds documents which match any of the subqueries.
* `_and`          - Finds documents which match all of the subqueries.
* `_not`          - Finds documents which don't match any of the subqueries.
* `_and_not`      - Finds documents which match the first subquery A but
                    not subquery B.
* `_xor`          - Finds documents which are matched by subquery A or other or
                    subquery B, but not both.

Each operator produces a weight for each document it matches, which depends on
the weight of one or both subqueries in the following way:

* `_or`           - Matches documents with the sum of all weights of the subqueries.
* `_and`          - Matches documents with the sum of all weights of the subqueries.
* `_not`          - Finds documents which don't match any of the subqueries.
* `_and_not`      - Matches documents with the weight from subquery A only.

#### Example

For example, the following matches all of those who either like _bananas_ or
are _brown-eyed females_:

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
            { "gender": "female" },
            { "eyeColor": "brown" }
        ]
      }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Maybe

In addition to the basic logical operators, there is an additional logical
operator `_and_maybe` which matches any document which matches A (whether or
not B matches). If only B matches, then `_and_maybe` doesn't match. For this
operator, the weight is the sum of the matching subqueries, so:

* `_and_maybe`    - Finds any document which matches the first element of the
                    array and whether or not matches the rest.

1. Documents which match A and B match with the weight of A+B
2. Documents which match A only match with weight of A

This allows you to state that you require some terms (A) and that other
terms (B) are useful but not required.

#### Example

For example, the following matches all of those who like _bananas_ and which
maybe are also are _brown-eyed females_. It will return _brown-eyed females_
who like _bananas_ first:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_and_maybe": [
      {
        "favoriteFruit": "banana"
      },
      {
        "_and": [
            { "gender": "female" },
            { "eyeColor": "brown" }
        ]
      }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Filtering

A query can be filtered by another query. There are two ways to apply a filter
to a query depending whether you want to include or exclude documents:

* `_filter`       - Matches documents which match both subqueries, but the
                    weight is only taken from the left subquery (in other
                    respects it acts like `_and`.
* `_and_not`      - Matches documents which match the left subquery but don’t
                    match the right hand one (with weights coming from the left
                    subquery)

#### Example

For example, the following matches all who like _bananas_ filtering the results
to those who also are _brown-eyed females_, but this filter doesn't affect
weights:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "_filter": [
      {
        "favoriteFruit": "banana"
      },
      {
        "_and": [
            { "gender": "female" },
            { "eyeColor": "brown" }
        ]
      }
    ]
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Elite Set

* `_elite_set`    - Pick the best N subqueries and combine with `_or`.


## Additional Operators

* `_max`          - Pick the maximum weight of any subquery.

<!--
* `_wildcard`     - Wildcard expansion.
* `_scale_weight` -
* `_synonym`      -
 -->



---



## Range Searches

The keyword `_range` matches documents where the given value is between the
given `_from` and `_to` fixed range (including both endpoints).

If you only use the keyword `_from` matches documents where the given value is
greater than or equal to a fixed value.

If you only use the keyword `_to` matches documents where the given value is
less than or equal a fixed value.

#### Example

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


## Near

Another commonly used operator is `NEAR`, which finds terms within 10 words
of each other in the current document, behaving like `_and` with regard to
weights, so that:

* Documents which match A within 10 words of B are matched, with weight of A+B

#### Example

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "adventurous NEAR ambitious"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Phrase

A phrase is surrounded with double quotes (`""`) and allows searching for a
specific exact phrase and returns only matches where all terms appear in the
document in the correct order, giving a weight of the sum of each term.
For example:

* Documents which match A followed by B followed by C gives a weight of A+B+C

{: .note .info}
**_Note_**<br>
When searching for phrases, _stop words_ do not apply.

{: .note .warning}
**_Caution_**<br>
Hyphenated words are also treated as phrases, as are cases such as filenames
and email addresses (e.g. `/etc/passwd` or `president@whitehouse.gov`)

#### Example

In the following example, we will retrieve documents with the exact phrase,
including the stop words `these`, `are`, `few`, `and` `far` and `between`.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "\"these days are few and far between\""
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


## Love and Hate

The `+` and `-` operators, select documents based on the presence or absence of
specified terms.

{: .note .info}
**_Note_**<br>
When using these operators, _stop words_ do not apply.


#### Example

The following matches all documents with the phrase _"adventurous nature"_ but
not _ambitious_; and:

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "personality": "\"adventurous nature\" -ambitious"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}


{: .note .warning}
**_Caution_**<br>
One thing to note is that the behaviour of the +/- operators vary depending on
the default operator used and the above examples assume that the default (`OR`)
is used.
