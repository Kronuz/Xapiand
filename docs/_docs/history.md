---
title: History
read_only: true
---


## 0.8.0 / 2018-11-21
{: #v0-8-0}

- Cluster mode enabled
- Adds replication and remote protocol


## 0.7.0 / 2018-09-21
{: #v0-7-0}

- Prometheus metrics exposed at /:metrics


## 0.6.2 / 2018-05-03
{: #v0-6-2}

- Fix restore command memory usage
- MERGE and PATCH now require the document already exists
- Fix HTTP protocol on malformed messages
- Added --flush-threshold command line option
- Renamed command line option --force-up to simply --force
- Fix HTTP returning of large object bodies
- Fix indexing documents with integer IDs
- Fix io::read() to always return requested size when available
- Fix date accuracies


## 0.6.1 / 2018-04-25
{: #v0-6-1}

- Parallel restore commands
- General optimizations
- Fixed geospatial terms and accuracy
- Fixes storage (no longer returns 500)
- Fixes multithreading bug with collected statistics


## 0.6.0 / 2018-04-11
{: #v0-6-0}

- Breaking: Support for multi-content (by Content-Type) documents
- Added :dump and :restore endpoints
- Fixes problem with big body responses breaking the logs
- Added time it took (in milliseconds) to execute a :search query
- Added sort, limit and offset to query DSL
- Python: Xapiand client updated
- Using C++17
- GCC 7 compatibility
- Updated FreeBSD port
- Towards MultiarchSpec compatibility for Linux


## 0.5.2 / 2018-04-02
{: #v0-5-2}

- Logo updated
- Fixes issues while compiling with GCC + cleanups


## 0.5.1 / 2018-03-28
{: #v0-5-1}

- Versioning of the project is now officially in the "0.x" line, after going
  through many ghost versions.
- Documentation was updated
- A Homebrew tap was created


## 0.5.0 / 2018-03-23
{: #v0-5-0}

- Site is up and running
- Documentation is finally cooking in the oven.
- This release also features a new website (based on Jekyll's theme)
- git@8cc67578b2


## 0.4.2 / 2018-02-16
{: #v0-4-2}

- Fixes issues with "already locked database"
- git@016d45cc6e


## 0.4.1 / 2017-10-02
{: #v0-4-1}

- First stable release
- Lots of optimizations
- git@144900a8


## 0.4.0 / 2016-11-14
{: #v0-4-0}

- Fixed a few issues
- git@a72995f7


## 0.3.0 / 2016-06-08
{: #v0-3-0}

- First beta release
- Working under FreeBSD
- git@461ce941


## 0.0.0 / 2015-02-20
{: #v0-0-0}

- Birthday!
