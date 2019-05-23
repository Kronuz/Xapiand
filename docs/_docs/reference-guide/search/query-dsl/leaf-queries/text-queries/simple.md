---
title: Simple Query
short_title: Simple
---

This is the standard query for performing **Text** queries, if you use the
`text` field type in your schema.

Additionally, text fields can be analyzed for a particular language if you
specify the `_language` keyword in the field in your schema. If this is the
case, a stemming algorithm will be used to process a linguistic normalization
in which the variant forms of a word are reduced to a single common form
(stem of the word). These stemmed words are the ones used during search.
By default the `_language` is empty and in that case the stemming is not used.

### Example

In this example, the field "_personality_" has been fully language analyzed
during indexation. The text in query is also to be analyzed and the analysis
process constructs a boolean query from the provided text. The default boolean
operator is `OR`, so in the following request the text will be split, analyzed
and stemmed and then joined with the `OR` operator to create a query.

This query example will match with any document with the word "**_responsive_**"
in the text field "**_personality_**" but will also match documents with the
word "**_response_**" because "_responsive_" is reduced to "**_response_**" by
the stemmig algorithm.

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": "responsive"
  }
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
pm.test("simple query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

{: .test }

```js
pm.test("simple query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

{: .test }

```js
pm.test("simple query value is valid", function() {
  var jsonData = pm.response.json();
  var expected = [477, 246, 274, 773, 523, 705, 272, 339, 631, 854];
  for (var i = 0; i < 10; ++i) {
      pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```

Resulting in documents containing the word "_responsive_" in any part of the
body of the "_personality_" field.
