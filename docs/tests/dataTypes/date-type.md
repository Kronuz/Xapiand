---
title: Date type
---

## Date type

#### Index date type format yyyy/mm/dd hh:mm:ss

{% capture req %}

```json
PUT /test/date/doc

{
  "date": "2015/01/01 12:10:30"
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
{% endcomment %}

#### Get date type format yyyy/mm/dd hh:mm:ss

{% capture req %}

```json
GET /test/date/doc
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
  pm.expect(jsonData.date).to.equal('2015-01-01T12:10:30');
});
```
{% endcomment %}

#### Index date type format yyyy-mm-dd

{% capture req %}

```json
PUT /test/date/doc

{
  "date": "2015-01-01"
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
{% endcomment %}

#### Get date type format yyyy-mm-dd

{% comment %}
```json
GET /test/date/doc
```

```js
pm.test("Response is success", function() {
  pm.response.to.be.success;
});
```

```js
pm.test("Value is valid", function() {
  var jsonData = pm.response.json();
  pm.expect(jsonData.date).to.equal('2015-01-01T00:00:00');
});
```
{% endcomment %}

## Index date type format yyyy-mm-ddThh:mm:ss

{% capture req %}

```json
PUT /test/date/doc

{
  "date": "2015-01-01T12:10:30Z"
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
{% endcomment %}

#### Get date type format yyyy-mm-ddThh:mm:ss

{% capture req %}

```json
GET /test/date/doc
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
  pm.expect(jsonData.date).to.equal('2015-01-01T12:10:30');
});
```
{% endcomment %}

####  Index date type format epoch

{% capture req %}

```json
PUT /test/date/doc

{
"date": 1420070400.001
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
{% endcomment %}

#### Get date type format epoch

{% capture req %}

```json
GET /test/date/doc
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
  pm.expect(jsonData.date).to.equal('2015-01-01T00:00:00.001');
});
```
{% endcomment %}
