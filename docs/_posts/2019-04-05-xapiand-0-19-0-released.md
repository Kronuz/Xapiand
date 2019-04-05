---
title: "Xapiand 0.19.0 Released"
date: "2019-04-01 06:45:00 -0600"
author: Kronuz
version: 0.19.0
categories: [release]
---

Intensive writes in a database that was being queried some times resulted in
false DatabaseCorruptError errors, fixed. JSON produced \xHH codes which are
not part of the JSON standard and some times had problems, removed feature.


## Changed
- **BREAKING**: Remove support for \xHH in json

## Fixed
- Fixed errors during heavy writes/reads
- Fixed race condition in restore indexer
