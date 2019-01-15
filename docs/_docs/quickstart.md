---
title: Quick-start
---

This guide will take you through the process of installing Xapiand and
familiarize you with the concepts that will allow you to use search and
storage indexes. **DON'T PANIC**, it will take just a few minutes.

---

## Installing and Running


### Using Homebrew under macOS

```sh
# Install with Homebrew:
~ $ brew install Kronuz/tap/xapiand

# Run in foreground with *very* verbose output:
~ $ xapiand -vvvv
```

### Using Docker

```sh
# Run with *very* verbose output:
docker run -p 8880:8880 --rm dubalu/xapiand -vvvv
```


You can also check the [Installation]({{ '/docs/installation/' | relative_url }})
section for more details.

---

## Indexing

Let's try and index some twitter like information. First, let's create a
twitter user, and add some tweets (the twitter index will be created
automatically):

{% capture req %}
```json
PUT /twitter/user/Kronuz?pretty

{
  "name" : "German M. Bravo"
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}
```json
PUT /twitter/tweet/1?pretty

{
  "user": "Kronuz",
  "postDate": "2016-11-15T13:12:00",
  "message": "Trying out Xapiand, so far, so good... so what!"
}
```
{% endcapture %}
{% include curl.html req=req %}


{% capture req %}
```json
PUT /twitter/tweet/2?pretty

{
  "user": "Kronuz",
  "postDate": "2016-10-15T10:31:18",
  "message": "Another tweet, will it be indexed?"
}
```
{% endcapture %}
{% include curl.html req=req %}

You can dig a little deeper in the [Modifying Your Data]({{ '/docs/modifying/' | relative_url }}) section.

---

## Searching

Now, let's see if the information that was added by GETting it:

{% capture req %}
```json
GET /twitter/user/Kronuz?pretty
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}
```json
GET /twitter/tweet/1?pretty
```
{% endcapture %}
{% include curl.html req=req %}

{% capture req %}
```json
GET /twitter/tweet/2?pretty
```
{% endcapture %}
{% include curl.html req=req %}

Let's find all the tweets that Kronuz posted:

{% capture req %}
```json
GET /twitter/tweet/:search?q=user:Kronuz&pretty
```
{% endcapture %}
{% include curl.html req=req %}

You can find out more in the [Exploring Your Data]({{ '/docs/exploring/' | relative_url }}) section.

---

## Where to go from here?

Xapiand is both a simple and complex product. We've so far learned the basics
of what it is, how to look inside of it, and how to work with it using some of
the REST APIs. Hopefully this guide has given you a better understanding of
what Xapiand is and more importantly, inspired you to further experiment with
the rest of its great features!

Tutorials can be found in the [Tutorials]({{ '/tutorials/' | relative_url }})
section.
