---
title: ChangeLog
read_only: true
permalink: /changelog/
---

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/){:target="_blank"},
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html){:target="_blank"}.


---
## [Unreleased]
{: #unreleased }


---
## [0.25.0] - 2019-07-11
{: #v0-25-0 }

### Added
- Added support for `_meta` inside schemas (to allow user to add custom meta
data associated with it)

### Changed
- **BREAKING**: Schema declaration no longer needs nested `_schema` -> `schema`
                node; it now only needs `_schama`
- **BREAKING**: Strict mode requires newly created indexes to declare `_settings`
                with at least `number_of_shards` and `number_of_replicas`.
- **BREAKING**: Foreign schemas no longer use `_endpoint`, it was renamed to
                `_foreign`.

### Fixed
- Aggregations (min/max) which have no elements could result in errors.
- Fix parser query params to qdsl for not operator


---
## [0.24.0] - 2019-07-10
{: #v0-24-0 }

### Changed
- **BREAKING**: Clustering: Added primary shard promotions
- Updated xapian-core to latest 1.5 (git@db790e9e12bb9b3ebeaf916ac0acdea9a7ab0dd1)
- Python client updated
- Quarantine (invalid) WAL and database files/directories when appropriate
- Only nodes marked as primary for shards get the replica, others simply erase
  (or quarantine) stalled databases.
- Unused server sockets are now fully closed during shutdown

### Fixed
- **BREAKING**: Clustering: Increased overall cluster stability
- Remote protocol: Race conditions for some used database objects after checkin
- Refactored node shutdown process
- Schemas LRU properly detect changes to schemas now checking only inside
  "schema" and "description"
- WAL had a bug which could lead to skipped slots (it now checks revision before
  writing lines and recovers after errors)
- Many solo mode (and CLUSTERING=off) fixes.


---
## [0.23.0] - 2019-05-24
{: #v0-23-0 }

### Changed
- **BREAKING**: Geo: Renamed `height` to `altitude`
- **BREAKING**: Geo: Renamed `lng` to `lon`

### Fixed
- Schema with nested arrays and objects


---
## [0.22.2] - 2019-05-16
{: #v0-22-2 }

### Added
- Support for 'float' as an alternative name to 'floating' datatype

### Fixed
- Undefined behavior in UUID, serializer and term generator
- Error when requesting invalid types


---
## [0.22.1] - 2019-05-16
{: #v0-22-1 }

### Changed
- Performance improvements with restore by launching parallel DocIndexers
- Queries using glob wildcards

### Fixed
- Nested arrays and objects in arrays
- Restoring for GCC compiled Xapiand


---
## [0.22.0] - 2019-05-10
{: #v0-22-0 }

### Changed
- **BREAKING**: Added _version to document data
- **BREAKING**: Non-hashed prefixes for fields
- Indexing optimizations (_id and _version values)
- RESTORE now doesn't check _version

### Fixed
- Concurrency issues
- Issues with document version checks
- Fuzzy Queries fixed


---
## [0.21.2] - 2019-05-06
{: #v0-21-2 }

### Changed
- Enabled Honey backend

### Fixed
- Issues when launching multiple nodes simultaneously
- Retry database initialization during node launch


---
## [0.21.1] - 2019-05-05
{: #v0-21-1 }

### Fixed
- Fix compiler warning about DropAction/GetAction "changing meaning"


---
## [0.21.0] - 2019-05-04
{: #v0-21-0 }

### Changed
- Default do check_at_least one when using aggregations
- Updated xapian-core, fixes issues with leaking sockets
- **BREAKING**: Added patches for xapian-core to handle parallelized queries

### Fixed
- Fix issues when starting servers simultaneously


---
## [0.20.1] - 2019-04-29
{: #v0-20-1 }

### Fixed
- Fix issue with aggregations returning invalid results at times


---
## [0.20.0] - 2019-04-27
{: #v0-20-0 }

### Fixed
- **BREAKING**: Remote Protocol issue which provoked messaged to get out of sync
- Issues schemas not being saved (unexpected "_id" errors)
- Detection of remote schema updates
- Certain cluster remote queries


---
## [0.19.7] - 2019-04-15
{: #v0-19-7 }

### Added
- Experimental parsing of YAML
- Postman collection for testing documentation requests

### Changed
- Require CMake 3.12 (again)
- Docker image using Alpine 3.9, GCC 8 and ICU

### Fixed
- Check for requests with invalid body
- Compilation with cluster disabled


---
## [0.19.6] - 2019-04-13
{: #v0-19-6 }

### Fixed
- Casts


---
## [0.19.5] - 2019-04-13
{: #v0-19-5 }

### Fixed
- Throttling of commits (exception was being thrown)


---
## [0.19.4] - 2019-04-13
{: #v0-19-4 }

### Fixed
- Compilation using cmake 3.5 (use static libraries instead of object libraries)
- Regression which slowed down bulk indexing


---
## [0.19.3] - 2019-04-12
{: #v0-19-3 }

### Added
- GCC 7 and GCC 8 support (and libstdc++)

### Changed
- Refactored pfh module
- Using ENUM() with minimal perfect hash functions (improves performance)
- Upgraded xapian-core to latest 1.5 (git@84ccecdfb8938b4011aac0bf9539c139204acd22)


---
## [0.19.2] - 2019-04-09
{: #v0-19-2 }

### Changed
- Colorized INFO
- Tracebacks with normalized address
- Require CMake 3.12

### Fixed
- Use nanosleep instead of usleep and libev checking for EINTR
- Fix warning about available files under OS X


---
## [0.19.1] - 2019-04-08
{: #v0-19-1 }

### Added
- Added CJK NGRAM and CJK words as parameters to 'text' datatype
- Enable fuzzy searches in queries: use '~'. E.g. "unserten~3" would expand to "uncertain"

### Changed
- Ignore XAPIAN_* environment variables
- INFO signal showing busy threads callstacks, when tracebacks are enabled
- All exceptions now contain tracebacks, when tracebacks are enabled
- Hash for resolving cluster node for indexes skipping slashes

### Fixed
- Fix ranges in query
- Fix restoring from an empty request
- Formatting applied to saved dates


---
## [0.19.0] - 2019-04-05
{: #v0-19-0 }


### Changed
- **BREAKING**: Remove support for \xHH in json

### Fixed
- Fixed errors during heavy writes/reads
- Fixed race condition in restore indexer


---
## [0.18.1] - 2019-04-02
{: #v0-18-1 }

### Fixed
- Fixed logging for verbose request/responses


---
## [0.18.0] - 2019-04-01
{: #v0-18-0 }

### Added
- The `?echo` query param returns the new/updated object

### Changed
- **BREAKING**: By default, write/update operations no longer return the object (see `?echo`)
- **BREAKING**: Removed `:schema`, schemas are now added/edited by using the
                Create Index API.
- Verbosity level 4 or higher (`-vvvv`) turns on `--echo`, `--pretty` and `--comments`

### Fixed
- Replication protocol now properly closing connections on errors


---
## [0.17.0] - 2019-03-28
{: #v0-17-0 }

### Added
- Set metadata
- Added `PUT` index

### Changed
- **BREAKING**: Removed `:touch`

### Fixed
- HTTP Mappings
- Schemas


---
## [0.16.1] - 2019-03-26
{: #v0-16-1 }

### Fixed
- Selectors were not working
- Mime Types

---
## [0.16.0] - 2019-03-26
{: #v0-16-0 }

### Added
- Added --pretty (and --no-pretty) options (for default `?pretty`)
- Verbosity also toggles "prettiness"
- HTTP method mapping + override
- TravisCI builds, thanks to Vitold S. (@vit1251)

### Fixed
- Fixed `_version` checks


---
## [0.15.0] - 2019-03-21
{: #v0-15-0 }

### Added
- Sharding support added
  + By default there are now five shards per index
  + Shards are put inside subdirectories with the shard number prefixed with `.__`

### Changed
- **BREAKING**: All schemas are now foreign by default, and get put inside the databases indexes
- **BREAKING**: Databases indexes directories work like shards and thus were renamed
                from `.xapiand/<node_name>` to `.xapiand/index/.__<node_idx>`


---
## [0.14.0] - 2019-03-12
{: #v0-14-0 }

### Changed
- **BREAKING**: Cluster directory renamed from `.cluster` to `.xapiand`;
- **BREAKING**: Indexes directories renamed from `.index/<node_idx>` to `.xapiand/<node_name>`
- **BREAKING**: Upgraded xapian-core to latest 1.5 (master)


---
## [0.12.2] - 2019-03-11
{: #v0-12-2 }

### Fixed
- Fix regression introduced in 0.12.1 during cluster bootstrapping


---
## [0.12.1] - 2019-03-11
{: #v0-12-1 }

### Fixed
- Fix requests resulting in Bad Gateway with stalled endpoints

### Changed
- Renamed options for logging from `--log-*` to `--log <setting>`


---
## [0.12.0] - 2019-03-10
{: #v0-12-0 }

### Fixed
- Aggregations with `_aggs` in different order
- JSON serialization of Infinite and NaN
- Fixed race condition during bulk indexing

### Changed
- **BREAKING**: Fields in responses renamed and nested differently:
                + `query.hits` moved to `hits`
                + `query.matches_estimated` renamed to `count`
                + `query.total_count` renamed to `doc_count`
- `MERGE` method is deprecated and will be removed, use `UPDATE` instead
- Default UUID mode is now encoded + compact


---
## [0.11.1] - 2019-03-06
{: #v0-11-1 }

### Fixed
- Fix Docker build

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


[Unreleased]: https://github.com/Kronuz/Xapiand/compare/v0.25.0...HEAD
[0.25.0]: https://github.com/Kronuz/Xapiand/compare/v0.24.0...v0.25.0
[0.24.0]: https://github.com/Kronuz/Xapiand/compare/v0.23.0...v0.24.0
[0.23.0]: https://github.com/Kronuz/Xapiand/compare/v0.22.2...v0.23.0
[0.22.2]: https://github.com/Kronuz/Xapiand/compare/v0.22.1...v0.22.2
[0.22.1]: https://github.com/Kronuz/Xapiand/compare/v0.22.0...v0.22.1
[0.22.0]: https://github.com/Kronuz/Xapiand/compare/v0.21.2...v0.22.0
[0.21.2]: https://github.com/Kronuz/Xapiand/compare/v0.21.1...v0.21.2
[0.21.1]: https://github.com/Kronuz/Xapiand/compare/v0.21.0...v0.21.1
[0.21.0]: https://github.com/Kronuz/Xapiand/compare/v0.20.1...v0.21.0
[0.20.1]: https://github.com/Kronuz/Xapiand/compare/v0.20.0...v0.20.1
[0.20.0]: https://github.com/Kronuz/Xapiand/compare/v0.19.7...v0.20.0
[0.19.7]: https://github.com/Kronuz/Xapiand/compare/v0.19.6...v0.19.7
[0.19.6]: https://github.com/Kronuz/Xapiand/compare/v0.19.5...v0.19.6
[0.19.5]: https://github.com/Kronuz/Xapiand/compare/v0.19.4...v0.19.5
[0.19.4]: https://github.com/Kronuz/Xapiand/compare/v0.19.3...v0.19.4
[0.19.3]: https://github.com/Kronuz/Xapiand/compare/v0.19.2...v0.19.3
[0.19.2]: https://github.com/Kronuz/Xapiand/compare/v0.19.1...v0.19.2
[0.19.1]: https://github.com/Kronuz/Xapiand/compare/v0.19.0...v0.19.1
[0.19.0]: https://github.com/Kronuz/Xapiand/compare/v0.18.1...v0.19.0
[0.18.1]: https://github.com/Kronuz/Xapiand/compare/v0.18.0...v0.18.1
[0.18.0]: https://github.com/Kronuz/Xapiand/compare/v0.17.0...v0.18.0
[0.17.0]: https://github.com/Kronuz/Xapiand/compare/v0.16.1...v0.17.0
[0.16.1]: https://github.com/Kronuz/Xapiand/compare/v0.16.0...v0.16.1
[0.16.0]: https://github.com/Kronuz/Xapiand/compare/v0.15.0...v0.16.0
[0.15.0]: https://github.com/Kronuz/Xapiand/compare/v0.14.0...v0.15.0
[0.14.0]: https://github.com/Kronuz/Xapiand/compare/v0.12.2...v0.14.0
[0.12.2]: https://github.com/Kronuz/Xapiand/compare/v0.12.1...v0.12.2
[0.12.1]: https://github.com/Kronuz/Xapiand/compare/v0.12.0...v0.12.1
[0.12.0]: https://github.com/Kronuz/Xapiand/compare/v0.11.1...v0.12.0
[0.11.1]: https://github.com/Kronuz/Xapiand/compare/v0.11.0...v0.11.1
[0.11.0]: https://github.com/Kronuz/Xapiand/compare/v0.10.2...v0.11.0
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
