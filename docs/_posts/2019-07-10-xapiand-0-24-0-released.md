---
title: "Xapiand 0.24.0 Released"
date: "2019-07-10 15:37:00 -0600"
author: Kronuz
version: 0.24.0
categories: [release]
---

This release fixes a lot of issues with the cluster and improves cluster
stability and reliability. Primary shards get elected and promoted from replicas
when there is quorum and at least 5 minutes (by default) have passed since the
primary shard's node has been seen (this happens only for shards being actively
accessed).

Files are now quarantined instead of deleted; this can happen when WAL gets
corrupted, a stalled replica database is detected (not marked as replica but
with a copy of the database), etc.


### Changed
- **BREAKING**: Clustering: Added primary shard promotions
- **BREAKING**: Updated xapian-core to latest 1.5 (git@db790e9e12bb9b3ebeaf916ac0acdea9a7ab0dd1)
- Python client updated
- Quarantine (invalid) WAL and database files/directories when appropriate
- Only nodes marked as primary for shards get the replica, others simply erase
  (or quarantine) stalled databases.
- Unused server sockets are now fully closed during shutdown

### Fixed
- **BREAKING**: Clustering: Increased overall cluster stability
- Remote protocol: Race conditions for some used database objects after checkin
- Refactored node shutdown process
- Schemas LRU properly detect changes to schemas now checking only inside
  "schema" and "description"
- WAL had a bug which could lead to skipped slots (it now checks revision before
  writing lines and recovers after errors)
- Many solo mode (and CLUSTERING=off) fixes.
