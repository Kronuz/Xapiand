---
title: "Xapiand 0.11.0 Released"
date: "2019-03-05 15:40:00 -0600"
author: Kronuz
version: 0.11.0
categories: [release]
---

This version changes the response fields, it removes fields starting with '#',
which could be considered a breaking. Cluster .index is now also sharded and
adds searching on multiple indexes. Scripts are working again, with a simplified
scripting language and support for Foreign Scripts.


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
