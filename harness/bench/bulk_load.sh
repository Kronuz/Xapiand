#!/usr/bin/env bash
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

# bulk_load.sh ARM [--trials N] [--datasets a,b] [--bin BIN] [--out JSON] [--fetch-only]
#
# The 2018 Kibana bulk loads (shakespeare 111,396 docs, logs 14,005, accounts 1,000) on this host, one shard, no replicas,
# each timed to its durable acknowledgment; ARM is one of
#   xapiand        BIN with `--shards 1`, one `RESTORE /idx/?commit` of es-*.msgpack: the response comes after the strata commit
#                  (fsync and the barrier fsync of macOS, docs/design/phase7c.md D7)
#   xapiand-full   the same with `--full-flush-ordering` (F_FULLFSYNC on macOS)
#   es             Elasticsearch, one `_bulk?refresh=wait_for` of the whole *-2.json: acknowledged after the translog fsync
#                  (index.translog.durability=request) and visible after a refresh
#   es-chunked     Elasticsearch, `_bulk` in requests of 5,000 documents, the last with refresh=wait_for
# Each trial starts the engine fresh on an empty directory, loads one dataset and stops it; prints the median and spread.
# Pinned: the Elasticsearch tarball below (version and SHA-512); nothing else is downloaded. Scratch (the tarball, its
# extraction, decompressed datasets, data directories) is under $BULK_SCRATCH, default ~/.scratch/xapiand/bulk-load-rerun.
set -eu
. "$(dirname "$0")/../env.sh"
export ES_VERSION=9.5.4
export ES_SHA512=ef7bbfabf2dab4190c83239cc3f663e3bdb729258acdf6e090325a271ccec5943c4f1ce0b06a9b0c7b1e793cfeca43510e5cbd3447d0f4bc8af7e0c1fe3866b3
export ES_URL=https://artifacts.elastic.co/downloads/elasticsearch/elasticsearch-$ES_VERSION-darwin-x86_64.tar.gz
export BULK_SCRATCH=${BULK_SCRATCH:-$HOME/.scratch/xapiand/bulk-load-rerun}
export BULK_DATASETS=${BULK_DATASETS:-$HOME/Development/Kronuz/datasets/kibana-2018}
[ $# -ge 1 ] || { sed -n 2,3p "$0" | cut -c3-; exit 2; }
exec "$PY" -I "$(dirname "$0")/bulk_load.py" "$@"
