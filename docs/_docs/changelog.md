---
title: ChangeLog
read_only: true
---

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/){:target="_blank"},
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html){:target="_blank"}.


---
## [Unreleased]
{: #unreleased }

### Added
- Added support to routing key hints in the API, by using `routing` query param


---
## [0.11.0] - 2019-03-05
{: #v0-11-0 }

### Fixed
- Cluster replication is now functional, it was broken in 0.10.0
- Fix searching in "_id" field
- Fixed and documented Scripts and Foreign Scripts

### Changed
- **BREAKING**: `.index` shared by node
- **BREAKING**: Keyword datatype term prefix is now 'K'
- **BREAKING**: Guessed IDs are now strings also for numeric IDs
- **BREAKING**: URL drill selector now uses dot ('.') directly, so document IDs
                cannot have dots in them now.
- **BREAKING**: System-added fields in returned objects are no longer prefixed by '#'
- Never overwrite an existent database
- Index only adds new indexes during write operations
- Do not save empty data inside database
- Numeric IDs are auto-incremented when creating new documents with `POST`

### Added
- Added '*'' for using as multiple indexes


---
## [0.10.2] - 2019-02-20
{: #v0-10-2 }

### Fixed
- Docker build had missing symbols since 0.10.0
- Fix exit code on errors


---
## [0.10.1] - 2019-02-20
{: #v0-10-1 }

### Fixed
- Default database path is /var/run/xapiand


---
## [0.10.0] - 2019-02-20
{: #v0-10-0 }

### Added
- Added `_match_all` and `_match_none` to `:search` command
- Added the Elite Set Operator

### Changed
- **BREAKING**: Cluster using `.index` and `.cluster` databases
- **BREAKING**: Indexes paths (endpoints) no longer have a trailing slash
- Use OP_FILTER for terms in range queries
- Flattened range queries
- The default stopper during indexation is now `stop_stemmed`
- Numeric ranges are now properly calculating the filtering terms
- Geospatial queries are now calculating better the filtering trixels terms
- Default accuracies changed
- Removed `_raw` from QueryDSL

### Fixed
- Fixed `sort` query param during search


---
## [0.9.0] - 2019-02-11
{: #v0-9-0 }

### Added
- Embedded Xapian core 1.5.0 in the source code
- Accept application/x-ndjson as input for `:restore` command
- Home and docs site with cosmetic improvements

### Changed
- Greatly improves performance
- Documentation extended
- Upgraded to fmtlib v5.3.1 library
- Upgraded to ChaiScript v6.1.1 library
- Sort accepting casts
- QueryDSL with flattened queries

### Fixed
- Fixes a few issues with the way geospatial locations were
  being filtered and indexed


---
## [0.8.15] - 2019-02-01
{: #v0-8-15 }

### Added
- Keep-Alive and Close connection support

### Changed
- Compress documents using LZ4 instead of gzip
- Text fields no longer remove terms with Stopper

### Fixed
- Fix bug with UUIDs and macOS


---
## [0.8.14] - 2019-01-27
{: #v0-8-14 }

### Added
- Added new Xapiand client (based on Elasticsearch client)
- Added sorting for Aggregations
- Added `_keyed` to aggregations optional. Keyed are off by default now
- Added `_shift` to aggregations

### Deprecated
- Schema deprecated `string` type, it now only has `text` and
  `keyword` (`string` will be removed at some point)


---
## [0.8.13] - 2019-01-21
{: #v0-8-13 }

### Added
- Added bounds to standard deviation aggregation
- Documentation about Aggregations

### Changed
- Remote and Replication protocol are now split in two

### Fixed
- Range keys for aggregations fixed


---
## [0.8.12] - 2019-01-15
{: #v0-8-12 }

### Changed
- Documentation updated

### Fixed
- Fix Docker entrypoint


---
## [0.8.11] - 2019-01-14
{: #v0-8-11 }

### Changed
- Uses `/var/db/xapiand` directory for database by default


---
## [0.8.10] - 2019-01-10
{: #v0-8-10 }

### Changed
- Default bind address is now again 0.0.0.0


---
## [0.8.9] - 2019-01-10
{: #v0-8-9 }

### Fixed
- Fixes to multicast script for Kubernetes


---
## [0.8.8] - 2018-12-27
{: #v0-8-8 }

### Added
- Added daemon script for multicasting across k8s nodes

### Changed
- UDP usage tweaked (for Discovery)
- Default TCP bind address is now the current IP


---
## [0.8.7] - 2018-12-21
{: #v0-8-7 }

### Fixed
- Explicit initialization of schema for cluster database
- Cluster database endpoints normalized


---
## [0.8.6] - 2018-12-20
{: #v0-8-6 }

### Changed
- Using fnv1ah64 for clustering hash (`jump_consistent_hash`)


---
## [0.8.5] - 2018-12-20
{: #v0-8-5 }

### Changed
- Logo adjustments + banner updated


---
## [0.8.4] - 2018-12-16
{: #v0-8-4 }

### Fixed
- Fix Remote storage
- Discovery protocol fixed issues with cluster


---
## [0.8.3] - 2018-12-19
{: #v0-8-3 }

### Changed
- Discovery/Raft: Merged both into Discovery

### Fixed
- Colorize all node names
- Fix exit codes


---
## [0.8.2] - 2018-12-16
{: #v0-8-2 }

### Changed
- Improve efficiency of nodes update after PING


---
## [0.8.1] - 2018-12-13
{: #v0-8-1 }

### Changed
- Rewrite of Database Pool
- Thread names changed


---
## [0.8.0] - 2018-11-21
{: #v0-8-0 }

### Added
- Cluster mode enabled
- Adds replication and remote protocol


---
## [0.7.0] - 2018-09-21
{: #v0-7-0 }

### Added
- Prometheus metrics exposed at `/:metrics`


---
## [0.6.2] - 2018-05-03
{: #v0-6-2 }

### Added
- Added `--flush-threshold` command line option

### Changed
- `MERGE` and `PATCH` now require the document already exists
- Renamed command line option `--force-up` to simply `--force`

### Fixed
- Fix restore command memory usage
- Fix HTTP protocol on malformed messages
- Fix HTTP returning of large object bodies
- Fix indexing documents with integer IDs
- Fix io::read() to always return requested size when available
- Fix date accuracies


---
## [0.6.1] - 2018-04-25
{: #v0-6-1 }

### Added
- Parallel restore commands
- General optimizations

### Changed
- **BREAKING**: Geospatial fields changed generated terms. Reindexing needed!

### Fixed
- Fixed geospatial terms and accuracy
- Fixes storage (no longer returns 500)
- Fixes multithreading bug with collected statistics


---
## [0.6.0] - 2018-04-11
{: #v0-6-0 }

### Added
- Added :dump and :restore endpoints
- Added time it took (in milliseconds) to execute a :search query
- Added sort, limit and offset to query DSL
- Python: Xapiand client updated

### Changed
- **BREAKING**: Support for multi-content (by Content-Type) documents
- Using C++17
- Updated FreeBSD port

### Fixed
- GCC 7 compatibility
- Fixes problem with big body responses breaking the logs
- Towards MultiarchSpec compatibility for Linux


---
## [0.5.2] - 2018-04-02
{: #v0-5-2 }

### Changed
- Logo updated

### Fixed
- Fixes issues while compiling with GCC + cleanups


---
## [0.5.1] - 2018-03-28
{: #v0-5-1 }

### Added
- A Homebrew tap was created

### Changed
- Versioning of the project is now officially in the "0.x" line, after going
  through many ghost versions.
- Documentation was updated


---
## [0.5.0] - 2018-03-23
{: #v0-5-0 }

### Added
- Site is up and running
- Documentation is finally cooking in the oven.
- This release also features a new website (based on Jekyll's theme)


---
## [0.4.2] - 2018-02-16
{: #v0-4-2 }

### Fixed
- Fixes issues with "already locked database"


---
## [0.4.1] - 2017-10-02
{: #v0-4-1 }

### Changed
- First stable release
- Lots of optimizations


---
## [0.4.0] - 2016-11-14
{: #v0-4-0 }

### Fixed
- Fixed a few issues


---
## [0.3.0] - 2016-06-08
{: #v0-3-0 }

### Added
- First beta release

### Fixed
- Working under FreeBSD


---
## 0.0.0 - *2015-02-20*
{: #v0-0-0 }

- Birthday!


[Unreleased]: https://github.com/Kronuz/Xapiand/compare/v0.11.0...HEAD
[0.10.2]: https://github.com/Kronuz/Xapiand/compare/v0.10.2...v0.11.0
[0.10.2]: https://github.com/Kronuz/Xapiand/compare/v0.10.1...v0.10.2
[0.10.1]: https://github.com/Kronuz/Xapiand/compare/v0.10.0...v0.10.1
[0.10.0]: https://github.com/Kronuz/Xapiand/compare/v0.9.0...v0.10.0
[0.9.0]: https://github.com/Kronuz/Xapiand/compare/v0.8.15...v0.9.0
[0.8.15]: https://github.com/Kronuz/Xapiand/compare/v0.8.14...v0.8.15
[0.8.14]: https://github.com/Kronuz/Xapiand/compare/v0.8.13...v0.8.14
[0.8.13]: https://github.com/Kronuz/Xapiand/compare/v0.8.12...v0.8.13
[0.8.12]: https://github.com/Kronuz/Xapiand/compare/v0.8.11...v0.8.12
[0.8.11]: https://github.com/Kronuz/Xapiand/compare/v0.8.10...v0.8.11
[0.8.10]: https://github.com/Kronuz/Xapiand/compare/v0.8.9...v0.8.10
[0.8.9]: https://github.com/Kronuz/Xapiand/compare/v0.8.8...v0.8.9
[0.8.8]: https://github.com/Kronuz/Xapiand/compare/v0.8.7...v0.8.8
[0.8.7]: https://github.com/Kronuz/Xapiand/compare/v0.8.6...v0.8.7
[0.8.6]: https://github.com/Kronuz/Xapiand/compare/v0.8.5...v0.8.6
[0.8.5]: https://github.com/Kronuz/Xapiand/compare/v0.8.4...v0.8.5
[0.8.4]: https://github.com/Kronuz/Xapiand/compare/v0.8.3...v0.8.4
[0.8.3]: https://github.com/Kronuz/Xapiand/compare/v0.8.2...v0.8.3
[0.8.2]: https://github.com/Kronuz/Xapiand/compare/v0.8.1...v0.8.2
[0.8.1]: https://github.com/Kronuz/Xapiand/compare/v0.8.0...v0.8.1
[0.8.0]: https://github.com/Kronuz/Xapiand/compare/v0.7.0...v0.8.0
[0.7.0]: https://github.com/Kronuz/Xapiand/compare/v0.6.2...v0.7.0
[0.6.2]: https://github.com/Kronuz/Xapiand/compare/v0.6.1...v0.6.2
[0.6.1]: https://github.com/Kronuz/Xapiand/compare/v0.6.0...v0.6.1
[0.6.0]: https://github.com/Kronuz/Xapiand/compare/v0.5.2...v0.6.0
[0.5.2]: https://github.com/Kronuz/Xapiand/compare/v0.5.1...v0.5.2
[0.5.1]: https://github.com/Kronuz/Xapiand/compare/v0.5.0...v0.5.1
[0.5.0]: https://github.com/Kronuz/Xapiand/compare/v0.4.2...v0.5.0
[0.4.2]: https://github.com/Kronuz/Xapiand/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/Kronuz/Xapiand/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/Kronuz/Xapiand/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/Kronuz/Xapiand/compare/v0.0.0...v0.3.0
