---
title: "Xapiand 0.14.0 Released"
date: "2019-03-12 13:10:00 -0600"
author: Kronuz
version: 0.14.0
categories: [release]
---

Xapian core updated. Floating points are now packed the same way IEEE packs
them. Cluster and Index directories were moved (again) hopefully for the last
time, they are now in `.xapiand` subdirectory; this is towards adding support
for multiple shards per index.


### Changed
- **BREAKING**: Cluster directory renamed from `.cluster` to `.xapiand`;
- **BREAKING**: Indexes directories renamed from `.index/<node_idx>` to `.xapiand/<node_name>`
- **BREAKING**: Upgraded xapian-core to latest 1.5 (master)
