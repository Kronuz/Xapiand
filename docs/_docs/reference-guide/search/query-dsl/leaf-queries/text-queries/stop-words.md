---
title: Stop Words
---

Xapian supports a stop word list, which allows you to specify what words
should be removed from a query before processing. This list can be overridden
or stop words can still be searched for if desired, but by default any words
in the active stop words list will not be searched for.

### Example

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": "these days are few and far between"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{% comment %}

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Stop words are filtered from the query", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.total).to.equal(333);
});
```
{% endcomment %}

The stop words in the phrase (`these`, `are`, `and`) are removed before the
query runs, and only the remaining content words (`days`, `few`, `far`,
`between`) are searched. That is why documents match even though none of them
contain the whole phrase.


## Searching of Stop Words

Stop words can be searched by using [Love and Hate Modifiers](love-and-hate-modifiers)
(by adding `+` to the desired stop word) or by using an empty set of stopwords
in the `_stopwords` keyword:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": "+these days +are +few +and +far +between"
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
{% comment %}

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Searching of Stop Words results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(0);
  pm.expect(jsonData.hits.length).to.equal(0);
});
```
{% endcomment %}


The above example is equivalent to:

{: .note .unimplemented }
**_Unimplemented Feature!_**<br>
This feature hasn't yet been implemented...
<br>[Pull requests are welcome!]({{ site.repository }}/pulls){:target="_blank"}

{% capture req %}

```json
SEARCH /bank/

{
  "_query": {
    "personality": {
      "_value": "these days are few and far between",
      "_stopwords": []
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}
{% comment %}
---
status: 400
---
{% endcomment %}
