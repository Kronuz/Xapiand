---
title: Quick-start
---

This guide will take you through the process of installing Xapiand and
familiarize you with the concepts that will allow you to use the storage and
search indexes. **DON'T PANIC**, it will take just a few minutes.

---

## Indexing

Let's try and index some twitter like information. First, let's create a
twitter user, and add some tweets (the twitter index will be created
automatically):

```sh
~ $ curl -XPUT 'localhost:8880/twitter/user/Kronuz' -d '{
	"name" : "German M. Bravo"
}'

~ $ curl -XPUT 'localhost:8880/twitter/tweet/1' -d '{
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!"
}'

~ $ curl -XPUT 'localhost:8880/twitter/tweet/2' -d '{
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?"
}'
```

You can dig a little deeper in the [Modifying Your Data]({{ '/docs/modifying/' | relative_url }}) section.

---

## Searching

Now, let's see if the information that was added by GETting it:

```sh
~ $ curl 'localhost:8880/twitter/user/Kronuz?pretty'
~ $ curl 'localhost:8880/twitter/tweet/1?pretty'
~ $ curl 'localhost:8880/twitter/tweet/2?pretty'
```

Let's find all the tweets that Kronuz posted:

```sh
~ $ curl 'localhost:8880/twitter/tweet/:search?q=user:Kronuz&pretty'
```

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
