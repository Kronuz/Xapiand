---
title: "Xapiand 0.28.0 Released"
date: "2019-07-24 15:53:00 -0600"
author: Kronuz
version: 0.28.0
categories: [release]
---

Fixes restoring of documents in a clustered index.



### Changed
- **BREAKING**: `UPDATE` method no longer receives `upsert` query param
- **BREAKING**: Added `UPSERT` method
- **BREAKING**: Removed `MERGE` and `STORE` methods
- Restore always commits at the end

### Fixed
- Restoring of documents in a clustered index
- Commit in databases with remote shards (remote wasn't receiving the commit)
- Better handling of database flags (when changed)
- Uncompressed UUIDs representation was buggy