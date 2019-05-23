---
title: Default Operator
---

The default operator for text queries can be set by using the desired modifier.


## OR

_OR_ uses the `_or` modifier (the default).


## AND

_AND_ uses the `_and` modifier.

### Example

To make **AND** the default operator and thus forcing a query to search for
**all** terms instead of _any_ term (which is the default):

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_and": "these days are few and far between"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .test }

```js
pm.test("response is success", function() {
  pm.response.to.be.success;
});
```

{: .test }

```js
pm.test("Default operator count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

{: .test }

```js
pm.test("Default operator size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

{: .test }

```js
pm.test("Default operator values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [298, 958, 234, 923, 132, 567, 906, 116, 182, 600];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```


## Elite Set

_Elite Set_ uses the `_elite_set` modifier.

### Example

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_elite_set": "the biggest two things to know are that hes lovable and cooperative. Of course he's also kind, honest and considerate, but they're far less prominent, especially compared to impulses of being shallow as well"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
