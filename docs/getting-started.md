# Getting Started

This guide will take you through the process of installing Xapiand and
familiarize you with the concepts that will allow you to use the storage and
search indexes. **DON'T PANIC**, it will take just a few minutes.

---

# Installation

## Homebrew

Xapiand contains a formula for Homebrew (a package manager for OS X). It can
be installed by using the following command:

```sh
brew install --HEAD https://github.com/Kronuz/Xapiand/raw/master/contrib/homebrew/xapiand.rb
```

## Linux

```sh
(not available)
```


# First steps

## Indexing

Let's try and index some twitter like information. First, let's create a
twitter user, and add some tweets (the twitter index will be created
automatically):

```sh
curl -XPUT 'http://localhost:8880/twitter/user/Kronuz?commit' -d '{ "name" : "German M. Bravo" }'

curl -XPUT 'http://localhost:8880/twitter/tweet/1?commit' -d '{
    "user": "Kronuz",
    "postDate": "2016-11-15T13:12:00",
    "message": "Trying out Xapiand, so far, so good... so what!"
}'

curl -XPUT 'http://localhost:8880/twitter/tweet/2?commit' -d '{
    "user": "Kronuz",
    "postDate": "2016-10-15T10:31:18",
    "message": "Another tweet, will it be indexed?"
}'
```

Now, let’s see if the information was added by GETting it:

```sh
curl 'http://localhost:8880/twitter/user/Kronuz?pretty'
curl 'http://localhost:8880/twitter/tweet/1?pretty'
curl 'http://localhost:8880/twitter/tweet/2?pretty'
```

*TODO: Work in progress...*


## Searching

Let’s find all the tweets that Kronuz posted:

```sh
curl 'http://localhost:8880/twitter/tweet/.search?q=user:Kronuz&pretty'
```

*TODO: Work in progress...*


## Storage Engine

The storage is designed to put files in volumes much in the way Facebook's
Haystack <sup>[1](#footnote-1)</sup> works; once there a file enters the
storage it can't really get deleted/modified from the volume, but instead, if
a change is needed, a new file blob will be written to the volume. Storage is
envisioned to be used when there are files you need to store which you know
won't be changing often.

Lets put something in the storage using PUT:

```sh
curl -XPUT -H "Content-Type: image/png" 'http://localhost:8880/twitter/images/Kronuz.png?commit' --data-binary @'Kronuz.png'
```

And getting it is just a matter of retreiving it using GET:

```sh
curl -H "Accept: image/png" 'http://localhost:8880/twitter/images/Kronuz.png'
```

*TODO: Work in progress...*


# Where to go from here?

*TODO: Work in progress...*

---

<a id="footnote-1">1</a>: [Finding a needle in Haystack: Facebook's photo storage.](https://www.usenix.org/legacy/event/osdi10/tech/full_papers/Beaver.pdf)

<a id="footnote-2">2</a>: [The Little Engine(s) That Could: Scaling Online Social Networks.](http://ccr.sigcomm.org/online/files/p375.pdf)
