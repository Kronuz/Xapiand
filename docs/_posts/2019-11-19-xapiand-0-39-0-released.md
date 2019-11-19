---
title: "Xapiand 0.39.0 Released"
date: "2019-11-19 12:32:01 -0600"
author: Kronuz
version: 0.39.0
categories: [release]
---

Serialization the get_mset steps due the concurrency produce a segmentation fault.


### Fixed
- Segmentation fault in get_mset
- Added asserts on humanize inputs
- Added refs() funtion on shards
- Added assert on checkin for refs()
- Setting empty db on enquire inside get_mset for down db refs counter
- If prepared_mset always setting empty db in Enquire::Internal::set_database
- Fix re-use of readable databases
- Prevent locking forever, check pending every now and then