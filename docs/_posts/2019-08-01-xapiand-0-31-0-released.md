---
title: "Xapiand 0.31.0 Released"
date: "2019-08-01 12:10:00 -0600"
author: Kronuz
version: 0.31.0
categories: [release]
---

This release increses overall stability and makes logs less verbose (replication
protocol no longer logs operations, only when `--log replicas` is used.


### Changed
- No longer logging replication operations by default (it now requires `--log replicas`)

### Fixed
- Uncommon race condition while destroying clients
- Replicas no longer send db updated messages
- Always replicate databases from primary
