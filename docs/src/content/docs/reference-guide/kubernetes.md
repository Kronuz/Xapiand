---
title: "Kubernetes"
---

Because every Xapiand node keeps its own data on disk and discovers its peers to
form a cluster, it maps naturally onto a Kubernetes **StatefulSet**: each replica
gets a stable network identity and its own persistent volume, and a **headless
Service** gives the pods stable DNS names to find each other.

A node listens on three TCP ports, all of which the pods need to reach on each
other:

| Port | Protocol | Used for |
| ---: | --- | --- |
| `8880` | HTTP REST API | client requests |
| `9880` | Remote protocol | distributed searches across shards |
| `7880` | Replication protocol | keeping replicas in sync |

:::hint[Discovery uses UDP multicast.]{.caution}
Nodes find each other over UDP multicast, and many Kubernetes network plugins do
not forward multicast between pods by default. If your cluster's CNI does not
support it, discovery won't complete on its own; pin the interface each node
advertises with `--discovery-interface` (below) and use a CNI/network setup that
allows multicast between the pods of the StatefulSet.
:::

## A starting manifest

A three-node cluster with per-node storage. Each pod takes its node name from its
own pod name and advertises its own pod IP for discovery:

```yaml
apiVersion: v1
kind: Service
metadata:
  name: xapiand
spec:
  clusterIP: None          # headless: one stable DNS name per pod
  selector:
    app: xapiand
  ports:
    - { name: http,        port: 8880 }
    - { name: remote,      port: 9880 }
    - { name: replication, port: 7880 }
---
apiVersion: apps/v1
kind: StatefulSet
metadata:
  name: xapiand
spec:
  serviceName: xapiand
  replicas: 3
  selector:
    matchLabels:
      app: xapiand
  template:
    metadata:
      labels:
        app: xapiand
    spec:
      containers:
        - name: xapiand
          image: ghcr.io/kronuz/xapiand:latest
          args:
            - "--cluster=xapiand"
            - "--name=$(POD_NAME)"
            - "--discovery-interface=$(POD_IP)"
            - "--database=/data"
          env:
            - name: POD_NAME
              valueFrom: { fieldRef: { fieldPath: metadata.name } }
            - name: POD_IP
              valueFrom: { fieldRef: { fieldPath: status.podIP } }
          ports:
            - { name: http,        containerPort: 8880 }
            - { name: remote,      containerPort: 9880 }
            - { name: replication, containerPort: 7880 }
          volumeMounts:
            - { name: data, mountPath: /data }
  volumeClaimTemplates:
    - metadata:
        name: data
      spec:
        accessModes: ["ReadWriteOnce"]
        resources:
          requests:
            storage: 10Gi
```

The `--cluster` name must be the same on every node, `--name` must be unique (the
pod name satisfies this), and `--database` points at the mounted volume so each
node's data survives a restart or reschedule.

:::hint{.tip}
Size the `volumeClaimTemplates` storage for your data, and give each node enough
CPU and memory for indexing and search. To expose the REST API outside the
cluster, put a regular (non-headless) `Service` or an `Ingress` in front of port
`8880`; the remote and replication ports are only needed between the nodes.
:::
