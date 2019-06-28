---
title: Ignore Comments
---

## Ignore Comments


#### Ignore comment

{% capture req %}

```json
POST /test/comment-ignore/

{
  "_recurse": false,
  "_id": {
    "_type": "uuid",
  },
  "#comment": "This comment is ignored",
  "schema": {
    "_type": "object"
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
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  for (key in jsonData.schema) {
      pm.expect(key).to.not.include('#comment');
  }
});
```
{% endcomment %}