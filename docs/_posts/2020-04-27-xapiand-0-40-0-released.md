---
title: "Xapiand 0.40.0 Released"
date: "2020-04-27 15:00:00 -0600"
author: Kronuz
version: 0.40.0
categories: [release]
---

Fix update for caches when the schema is updated


### Added
- Added limits to caches without it
- Added missing Z to represent UTC on datetimes values
- Fix update outdated cache schemas from local and foreign schemas
- Fix update cache of index settings when the schema is updated
- Increase node lifespan and heartbeat timeout