---
title: ChangeLog
read_only: true
---

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

{: .note }
**_Types of changes_**<br>
  * `Added` for new features.<br>
  * `Changed` for changes in existing functionality.<br>
  * `Deprecated` for soon-to-be removed features.<br>
  * `Removed` for now removed features.<br>
  * `Fixed` for any bug fixes.<br>
  * `Security` in case of vulnerabilities.


## [Unreleased]
### Changed

- Use OP_FILTER for terms in range queries
- Flattened range queries



## [0.9.0] - 2019-02-11

- Greatly improves performance
- Embedded Xapian core 1.5.0 in the source code
- Fixes a few issues with the way geospatial locations were
  being filtered and indexed
- Documentation extended
- Sort accepting casts
- Upgraded to fmtlib v5.3.1 library
- Upgraded to ChaiScript v6.1.1 library
- QueryDSL with flattened queries
- Accept application/x-ndjson as input for `:restore` command
- Home and docs site with cosmetic improvements


## [0.8.15] - 2019-02-01

- Fix bug with UUIDs and macOS
- Compress documents using LZ4 instead of gzip
- Keep-Alive and Close connection support
- Text fields no longer remove terms with Stopper


## [0.8.14] - 2019-01-15

- Added new Xapiand client (based on Elasticsearch client)
- Schema dropped STRING, it now has TEXT and KEYWORD
- Added sorting for Aggregations
- Added `_keyed` to aggregations optional. Keyed are off by default now
- Added `_shift` to aggregations


## [0.8.13] - 2019-01-21

- Remote and Replication protocol are now split in two
- Added bounds to standard deviation aggregation
- Range keys for aggregations fixed
- Documentation about Aggregations

## [0.8.12] - 2019-01-15

- Documentation updated
- Fix Docker entrypoint


## [0.8.11] - 2019-01-14

- Uses /var/db/xapiand directory for database by default


## [0.8.10] - 2019-01-10

- Default bind address is now 0.0.0.0


## [0.8.9] - 2019-01-10

- Fixes to multicast script for Kubernetes


## [0.8.8] - 2018-12-27

- Added daemon script for multicasting across k8s nodes
- UDP usage tweaked (for Discovery)


## [0.8.7] - 2018-12-21

- Explicit initialization of schema for cluster database
- Cluster database endpoints normalized


## [0.8.6] - 2018-12-20

- Using fnv1ah64 for clustering hash (jump_consistent_hash)


## [0.8.5] - 2018-12-20

- Logo adjustments + banner updated


## [0.8.4] - 2018-12-16

- Fix Remote storage
- Discovery protocol fixed issues with cluster


## [0.8.3] - 2018-12-19

- Colorize all node names
- Fix exit codes
- Discovery/Raft: Merged both into Discovery


## [0.8.2] - 2018-12-16

- Improve efficiency of nodes update after PING


## [0.8.1] - 2018-12-13

- Rewrite of Database Pool
- Thread names changed


## [0.8.0] - 2018-11-21

- Cluster mode enabled
- Adds replication and remote protocol


## [0.7.0] - 2018-09-21

- Prometheus metrics exposed at /:metrics


## [0.6.2] - 2018-05-03

- Fix restore command memory usage
- MERGE and PATCH now require the document already exists
- Fix HTTP protocol on malformed messages
- Added --flush-threshold command line option
- Renamed command line option --force-up to simply --force
- Fix HTTP returning of large object bodies
- Fix indexing documents with integer IDs
- Fix io::read() to always return requested size when available
- Fix date accuracies


## [0.6.1] - 2018-04-25

- Parallel restore commands
- General optimizations
- Fixed geospatial terms and accuracy
- Fixes storage (no longer returns 500)
- Fixes multithreading bug with collected statistics


## [0.6.0] - 2018-04-11

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


## [0.5.2] - 2018-04-02

- Logo updated
- Fixes issues while compiling with GCC + cleanups


## [0.5.1] - 2018-03-28

- Versioning of the project is now officially in the "0.x" line, after going
  through many ghost versions.
- Documentation was updated
- A Homebrew tap was created


## [0.5.0] - 2018-03-23

- Site is up and running
- Documentation is finally cooking in the oven.
- This release also features a new website (based on Jekyll's theme)
- git@8cc67578b2


## [0.4.2] - 2018-02-16

- Fixes issues with "already locked database"
- git@016d45cc6e


## [0.4.1] - 2017-10-02

- First stable release
- Lots of optimizations
- git@144900a8


## [0.4.0] - 2016-11-14

- Fixed a few issues
- git@a72995f7


## [0.3.0] - 2016-06-08

- First beta release
- Working under FreeBSD
- git@461ce941


## [0.0.0] - 2015-02-20

- Birthday!
