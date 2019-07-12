---
title: "Xapiand 0.26.0 Released"
date: "2019-07-12 11:00:00 -0600"
author: Kronuz
version: 0.26.0
categories: [release]
---

Fixes Schema Metadata and error codes are now always returned (without `#`).


### Changed
- **BREAKING**: Errors returned with `status` and `message` (no hash `#`)

### Fixed
- Fixed `_meta` producing errors when passed to `POST`, `PUT` or `UPDATE`
