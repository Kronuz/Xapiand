---
title: "Xapiand 0.27.0 Released"
date: "2019-07-17 15:53:00 -0600"
author: Kronuz
version: 0.27.0
categories: [release]
---

This updates xapian-core to the latest 1.5, fixing queries with multiple shards
(when some shards had positions and some others didn't). Also fixes queries under
heavy concurrency using the remote protocol, which leaded to `Invalid MSG_GETMSET`
messages some times.

The STORE method has been removed, Storage API should now use `PUT` and `UPDATE`
instead.



### Changed
- **BREAKING**: `UPDATE` and `PATCH` don't create new objects, object must exist (or use `upsert` query param)
- **BREAKING**: `STORE` removed (use `PUT` or `UPDATE`)
- Updated xapian-core to latest 1.5 (git@00f69cf3928b44a756ddafcde248610f72babf62)

### Fixed
- Schemas not saving default schema as metadata some times
- Remote protocol matcher, for remote queries, now holds list for all pending queries.
