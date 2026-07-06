---
title: Search API
---

You can search for documents by using the `SEARCH` method:

{% capture req %}

```json
SEARCH /bank/
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
pm.test("Search API results", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.count).to.equal(10);
  pm.expect(jsonData.hits.length).to.equal(10);
});
```
{% endcomment %}


{: .note .tip }
It is also possible to use [HTTP Method Mappings]({{ '/docs/reference-guide/api#http-method-mapping' | relative_url }}).
