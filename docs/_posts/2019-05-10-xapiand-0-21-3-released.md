---
title: "Xapiand 0.22.0 Released"
date: "2019-05-10 13:49:00 -0600"
author: Kronuz
version: 0.22.0
categories: [release]
---

This release is the most stable/faster version ever! Additionally, it fixes
a couple remaining concurrency issues and increases indexing performance
by introducing optimizations about special fields *_id* and *_version*.


### Changed
- **BREAKING**: Added _version to document data
- **BREAKING**: Non-hashed prefixes for fields
- Indexing optimizations (_id and _version values)
- RESTORE now doesn't check _version

### Fixed
- Concurrency issues
- Issues with document version checks
- Fuzzy Queries fixed
