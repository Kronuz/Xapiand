---
title: Data Replication
---

Each index in Xapiand is divided into [shards]({{ '/docs/basic#shards' | relative_url }})
and each shard can have multiple copies. These copies are known as a replicas
and must be kept in sync when documents are added or removed. If we fail to do
so, reading from one copy will result in very different results than reading
from another. The process of keeping the shard copies in sync and serving reads
from them is what we call the data replication.

{: .note .construction }
_This section is a **work in progress**..._

<div style="min-height: 400px"></div>