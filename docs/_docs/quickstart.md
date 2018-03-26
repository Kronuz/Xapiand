---
title: Quick-start
permalink: "/docs/quickstart/"
---

This guide will take you through the process of installing Xapiand and
familiarize you with the concepts that will allow you to use the storage and
search indexes. **DON'T PANIC**, it will take just a few minutes.

---

# Indexing

Let's try and index some twitter like information. First, let's create a
twitter user, and add some tweets (the twitter index will be created
automatically):

```sh
~ $ curl -XPUT 'localhost:8880/twitter/user/Kronuz?commit' -d '{
	"name" : "German M. Bravo"
}'

~ $ curl -XPUT 'localhost:8880/twitter/tweet/1?commit' -d '{
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!"
}'

~ $ curl -XPUT 'localhost:8880/twitter/tweet/2?commit' -d '{
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?"
}'
```

Now, let’s see if the information was added by GETting it:

```sh
~ $ curl 'localhost:8880/twitter/user/Kronuz?pretty'
~ $ curl 'localhost:8880/twitter/tweet/1?pretty'
~ $ curl 'localhost:8880/twitter/tweet/2?pretty'
```

*TODO: Work in progress...*

---

# Searching

Let’s find all the tweets that Kronuz posted:

```sh
~ $ curl 'localhost:8880/twitter/tweet/.search?q=user:Kronuz&pretty'
```

*TODO: Work in progress...*

---

# Where to go from here?

*TODO: Work in progress...*

---

<sup><a id="footnote-1">1</a></sup> [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf){:target="_blank"}
