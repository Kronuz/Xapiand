---
title: "Xapiand 0.18.0 Released"
date: "2019-04-01 11:44:00 -0600"
author: Kronuz
version: 0.18.0
categories: [release]
---

Replication protocol fixed and response bodies are now not echoing created
or modified objects.


### Added
- The `?echo` query param returns the new/updated object

### Changed
- **BREAKING**: By default, write/update operations no longer return the object (see `?echo`)
- **BREAKING**: Removed `:schema`, schemas are now added/edited by using the
                Create Index API.
- Verbosity level 4 or higher (`-vvvv`) turns on `--echo`, `--pretty` and `--comments`

### Fixed
- Replication protocol now properly closing connections on errors
