---
title: Phrase Query
short_title: Phrase
---

A phrase is surrounded with double quotes (`""`) and allows searching for a
specific exact phrase and returns only matches where all terms appear in the
document in the correct order, giving a weight of the sum of each term.
For example:

* Documents which match A followed by B followed by C gives a weight of A+B+C

{: .note .info }
**_Note_**<br>
When searching for phrases, _stop words_ do not apply.

{: .note .caution }
**_Caution_**<br>
Hyphenated words are also treated as phrases, as are cases such as filenames
and email addresses (e.g. `/etc/passwd` or `president@whitehouse.gov`)

#### Example

In the following example, we will retrieve documents with the exact phrase,
including the stop words `these`, `are`, `few`, `and` `far` and `between`.

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": "\"these days are few and far between\""
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_phrase": "these days are few and far between"
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}
---
params: sort=_id
---

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("phrase query count", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
});
```

```js
pm.test("phrase query size", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.hits.length).to.equal(10);
});
```

```js
pm.test("phrase query values are valid", function() {
  var jsonData = pm.response.json();
  var expected = [26, 39, 44, 52, 60, 72, 82, 90, 119, 156];
  for (var i = 0; i < expected.length; ++i) {
    pm.expect(jsonData.hits[i]._id).to.equal(expected[i]);
  }
});
```
{% endcomment %}
