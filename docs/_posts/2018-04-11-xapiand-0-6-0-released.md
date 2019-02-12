---
title: "Xapiand 0.6.0 Released"
date: "2018-04-11 08:00:00 -0600"
author: Kronuz
version: 0.6.0
categories: [release]
---

This minor version bump contains changes that break the structure of the data;
it adds multi-content documents support, meaning you can now store multiple
contents (Content-Types) in a single document. See the
[Storage]({{ '/docs/reference-guide/storage/#multi-content-documents' | relative_url }})
reference guide for more details.

The Python client is updated as well to make it work with the new changes; this
makes it incompatible with older Xapiand servers. This is, hopefully, the last
breaking change needed for some time.

The release also fixes a few bugs and works towards better compatibility.

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
