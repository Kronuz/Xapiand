---
title: "Xapiand 0.15.0 Released"
date: "2019-03-21 15:50:00 -0600"
author: Kronuz
version: 0.15.0
categories: [release]
---

This release adds shards support, now Xapiand creates sharded databases by
default.


### Added
- Sharding support added
  + By default there are now five shards per index
  + Shards are put inside subdirectories with the shard number prefixed with `.__`

### Changed
- **BREAKING**: All schemas are now foreign by default, and get put inside the databases indexes
- **BREAKING**: Databases indexes directories work like shards and thus were renamed
                from `.xapiand/<node_name>` to `.xapiand/index/.__<node_idx>`
