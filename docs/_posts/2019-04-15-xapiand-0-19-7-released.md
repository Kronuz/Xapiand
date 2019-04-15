---
title: "Xapiand 0.19.7 Released"
date: "2019-04-15 16:20:00 -0600"
author: Kronuz
version: 0.19.7
categories: [release]
---

Docker image now using GCC 8 and Alpine 3.9. Experimental parsing of YAML,
although responses still return JSON-only objects.


### Added
- Experimental parsing of YAML
- Postman collection for testing documentation requests

### Changed
- Require CMake 3.12 (again)
- Docker image using Alpine 3.9, GCC 8 and ICU

### Fixed
- Check for requests with invalid body
- Compilation with cluster disabled
