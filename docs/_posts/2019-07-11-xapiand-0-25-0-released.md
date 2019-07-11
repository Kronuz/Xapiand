---
title: "Xapiand 0.25.0 Released"
date: "2019-07-11 15:57:00 -0600"
author: Kronuz
version: 0.25.0
categories: [release]
---

This release adds support for `_meta` in schemas. Schemas can now have custom
meta data associated with it. These are not used at all by Xapiand, but can be
used to store application-specific metadata, such as the class that a document
belongs to or schema descriptions.

Fortunately internal datatypes and structures are getting more stable, so this
is hopefully the last big breaking change for schemas.


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
