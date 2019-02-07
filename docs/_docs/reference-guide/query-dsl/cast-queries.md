---
title: Cast Query
---

Cast allow convert one type of data to another compatible type.

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "balance" : {
      "_integer": 221.46
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

In the above example cast 221.46 to integer

{% capture req %}

```json
GET /bank/:search?pretty

{
  "_query": {
    "contact.postcode" : {
      "_text": 43204
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

<table>
<colgroup>
<col width="40%" />
<col width="60%" />
</colgroup>
<thead>
<tr class="header">
<th>Types</th>
<th>Compatible types</th>
</tr>
</thead>
<tbody>
<tr>
<td markdown="span">**integer**</td>
<td markdown="span">**_positive** <br> **_float** <br> **_boolean** <br> **_text** </td>
</tr>
<tr>
<td markdown="span">**positive**</td>
<td markdown="span">**_integer** <br> **_float** <br> **_boolean** <br> **_text** </td>
</tr>
<tr>
<td markdown="span">**float**</td>
<td markdown="span">**_integer** <br> **_positive** <br> **_boolean** <br> **_text** </td>
</tr>
<tr>
<td markdown="span">**boolean**</td>
<td markdown="span">**_integer** <br> **_positive** <br> **_float** <br> **_text** </td>
</tr>
<tr>
<td markdown="span">**text**</td>
<td markdown="span">**_integer** <br> **_positive** <br> **_float** <br> **_boolean** <br> **_date** <br> **_time** <br> **_keyword** </td>
</tr>
<tr>
<td markdown="span">**date** <br>**time** <br>**geospatial** <br>**uuid**</td>
<td markdown="span">**_text**</td>
</tr>
</tbody>
</table>