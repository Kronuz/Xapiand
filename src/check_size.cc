/*
 * Copyright (c) 2018,2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "check_size.h"

#ifdef XAPIAND_CHECK_SIZES
#define STATIC_ASSERT(...)
// #define STATIC_ASSERT static_assert
#define CHECK_MAX_SIZE(max_size, name) \
	STATIC_ASSERT((max_size) >= (sizeof name), "Object is too big!"); \
	if ((max_size) < (sizeof name)) { \
		std::cerr << "sizeof" << #name << " = " << (sizeof name) << std::endl; \
	}

#include "allocator.h"
#include "base_x.hh"
#include "bloom_filter.hh"
#include "compressor_deflate.h"
#include "compressor_lz4.h"
#include "database/shard.h"
#include "database/handler.h"
#include "database/pool.h"
#include "database/schema.h"
#include "database/schemas_lru.h"
#include "database/wal.h"
#include "debouncer.h"
#include "endpoint.h"
#include "logger.h"
#include "manager.h"
#include "msgpack.h"
#include "node.h"
#include "query_dsl.h"
#include "queue.h"
#include "script.h"
#include "storage.h"
#include "threadpool.hh"
#include "url_parser.h"
#include "url_parser.h"
#include "aggregations/aggregations.h"
#include "aggregations/bucket.h"
#include "aggregations/metrics.h"
#include "booleanParser/BooleanParser.h"
#include "chaipp/chaipp.h"
#include "cuuid/uuid.h"
#include "geospatial/geometry.h"
#include "geospatial/geospatial.h"
#include "geospatial/intersection.h"
#include "geospatial/multicircle.h"
#include "geospatial/multiconvex.h"
#include "geospatial/multipoint.h"
#include "geospatial/multipolygon.h"
#include "geospatial/point.h"
#include "geospatial/polygon.h"
#include "metrics/basic_string_metric.h"
#include "metrics/jaccard.h"
#include "metrics/jaro.h"
#include "metrics/jaro_winkler.h"
#include "metrics/lcsubsequence.h"
#include "metrics/lcsubstr.h"
#include "metrics/levenshtein.h"
#include "metrics/sorensen_dice.h"
#include "metrics/soundex_metric.h"
#include "multivalue/geospatialrange.h"
#include "multivalue/keymaker.h"
#include "multivalue/range.h"
#include "phonetic/english_soundex.h"
#include "phonetic/french_soundex.h"
#include "phonetic/german_soundex.h"
#include "phonetic/spanish_soundex.h"
#include "server/base_client.h"
#include "server/remote_protocol.h"
#include "server/remote_protocol_client.h"
#include "server/remote_protocol_server.h"
#include "server/replication_protocol.h"
#include "server/replication_protocol_client.h"
#include "server/replication_protocol_server.h"
#include "server/buffer.h"
#include "server/discovery.h"
#include "server/http.h"
#include "server/http_client.h"
#include "server/http_server.h"
#include "server/raft.h"
#include "server/remote_protocol.h"
#include "server/replication_protocol.h"

#define TINY 8
#define SMALL 128
#define REGULAR 1024
#define BIG 5 * 1024
#define LARGE 20 * 1024

class DummyClient {
	ssize_t on_read(const char*, ssize_t) { return 0; }
	void on_read_file(const char*, ssize_t) {}
	void on_read_file_done() {}
};

void
check_size()
{

// allocator.h
CHECK_MAX_SIZE(TINY, (allocator::VanillaAllocator))
CHECK_MAX_SIZE(TINY, (allocator::TrackedAllocator))

// base_x.hh
CHECK_MAX_SIZE(TINY, (BaseX))
CHECK_MAX_SIZE(TINY, (Base2))
CHECK_MAX_SIZE(TINY, (Base8))
CHECK_MAX_SIZE(TINY, (Base11))
CHECK_MAX_SIZE(TINY, (Base16))
CHECK_MAX_SIZE(TINY, (Base32))
CHECK_MAX_SIZE(TINY, (Base36))
CHECK_MAX_SIZE(TINY, (Base58))
CHECK_MAX_SIZE(TINY, (Base59))
CHECK_MAX_SIZE(TINY, (Base62))
CHECK_MAX_SIZE(TINY, (Base64))
CHECK_MAX_SIZE(TINY, (Base66))

// bloom_filter.hh
CHECK_MAX_SIZE(SMALL, (BloomFilter<>))

// compressor_deflate.h
CHECK_MAX_SIZE(SMALL, (DeflateCompressData))
CHECK_MAX_SIZE(SMALL, (DeflateCompressFile))
CHECK_MAX_SIZE(SMALL, (DeflateDecompressData))
CHECK_MAX_SIZE(SMALL, (DeflateDecompressFile))

// compressor_lz4.h
CHECK_MAX_SIZE(SMALL, (LZ4CompressData))
CHECK_MAX_SIZE(SMALL, (LZ4CompressFile))
CHECK_MAX_SIZE(SMALL, (LZ4DecompressData))
CHECK_MAX_SIZE(SMALL, (LZ4DecompressFile))

// database_shard.h
CHECK_MAX_SIZE(SMALL, (Shard))

// database_handler.h
CHECK_MAX_SIZE(SMALL, (Data))
CHECK_MAX_SIZE(SMALL, (DatabaseHandler))
CHECK_MAX_SIZE(SMALL, (Document))
CHECK_MAX_SIZE(SMALL, (MSet))

// database_pool.h
CHECK_MAX_SIZE(SMALL, (ShardEndpoint))
CHECK_MAX_SIZE(SMALL, (DatabasePool))

// database_wal.h
CHECK_MAX_SIZE(BIG, (WalHeader))
CHECK_MAX_SIZE(TINY, (WalBinHeader))
CHECK_MAX_SIZE(TINY, (WalBinFooter))
CHECK_MAX_SIZE(SMALL, (DatabaseWAL))

// debouncer.h
// CHECK_MAX_SIZE(SMALL, (Debouncer<int, 0, 0, 0, void(*)(), int>))

// endpoint.h
CHECK_MAX_SIZE(SMALL, (Endpoint))
CHECK_MAX_SIZE(SMALL, (Endpoints))

// logger.h
CHECK_MAX_SIZE(SMALL, (Logging))

// manager.h
CHECK_MAX_SIZE(SMALL, (XapiandManager))

// msgpack.h
CHECK_MAX_SIZE(SMALL, (MsgPack))

// node.h
CHECK_MAX_SIZE(SMALL, (Node))

// query_dsl.h
CHECK_MAX_SIZE(SMALL, (QueryDSL))

// queue.h
CHECK_MAX_SIZE(SMALL, (queue::Queue<int>))

// remote_protocol.h
CHECK_MAX_SIZE(SMALL, (RemoteProtocol))

// replication.h
CHECK_MAX_SIZE(SMALL, (Replication))

// schema.h
CHECK_MAX_SIZE(SMALL, (Schema))

// schemas_lru.h
CHECK_MAX_SIZE(SMALL, (SchemasLRU))

// script.h
CHECK_MAX_SIZE(SMALL, (Script))

// storage.h
CHECK_MAX_SIZE(SMALL, (Storage<StorageHeader, StorageBinHeader, StorageBinFooter>))

// threadpool.hh
CHECK_MAX_SIZE(SMALL, (ThreadPool<>))

// url_parser.h
CHECK_MAX_SIZE(SMALL, (QueryParser))
CHECK_MAX_SIZE(SMALL, (PathParser))

// aggregations/aggregations.h
CHECK_MAX_SIZE(SMALL, (Aggregation))
CHECK_MAX_SIZE(SMALL, (AggregationMatchSpy))

// aggregations/bucket.h
CHECK_MAX_SIZE(SMALL, (BucketAggregation))
CHECK_MAX_SIZE(SMALL, (ValueAggregation))
CHECK_MAX_SIZE(SMALL, (HistogramAggregation))
CHECK_MAX_SIZE(SMALL, (RangeAggregation))
CHECK_MAX_SIZE(SMALL, (FilterAggregation))

// aggregations/metrics.h
CHECK_MAX_SIZE(SMALL, (ValueHandle))
CHECK_MAX_SIZE(SMALL, (HandledSubAggregation))
CHECK_MAX_SIZE(SMALL, (MetricCount))
CHECK_MAX_SIZE(SMALL, (MetricSum))
CHECK_MAX_SIZE(SMALL, (MetricAvg))
CHECK_MAX_SIZE(SMALL, (MetricMin))
CHECK_MAX_SIZE(SMALL, (MetricMax))
CHECK_MAX_SIZE(SMALL, (MetricVariance))
CHECK_MAX_SIZE(SMALL, (MetricStdDeviation))
CHECK_MAX_SIZE(SMALL, (MetricMedian))
CHECK_MAX_SIZE(SMALL, (MetricMode))
CHECK_MAX_SIZE(SMALL, (MetricStats))
CHECK_MAX_SIZE(SMALL, (MetricExtendedStats))

// booleanParser/BooleanParser.h
CHECK_MAX_SIZE(SMALL, (BooleanTree))

// cuuid/uuid.h
CHECK_MAX_SIZE(SMALL, (UUID))

// geospatial/geometry.h
CHECK_MAX_SIZE(SMALL, (Constraint))
CHECK_MAX_SIZE(SMALL, (Geometry))
// geospatial/geospatial.h
CHECK_MAX_SIZE(SMALL, (GeoSpatial))
// geospatial/intersection.h
CHECK_MAX_SIZE(SMALL, (Intersection))
// geospatial/multicircle.h
CHECK_MAX_SIZE(SMALL, (MultiCircle))
// geospatial/multiconvex.h
CHECK_MAX_SIZE(SMALL, (MultiConvex))
// geospatial/multipoint.h
CHECK_MAX_SIZE(SMALL, (MultiPoint))
// geospatial/multipolygon.h
CHECK_MAX_SIZE(SMALL, (MultiPolygon))
// geospatial/point.h
CHECK_MAX_SIZE(SMALL, (Point))
// geospatial/polygon.h
CHECK_MAX_SIZE(SMALL, (Polygon))

// metrics/basic_string_metric.h
CHECK_MAX_SIZE(SMALL, (Counter))
// metrics/jaccard.h
CHECK_MAX_SIZE(SMALL, (Jaccard))
// metrics/jaro.h
CHECK_MAX_SIZE(SMALL, (Jaro))
// metrics/jaro_winkler.h
CHECK_MAX_SIZE(SMALL, (Jaro_Winkler))
// metrics/lcsubsequence.h
CHECK_MAX_SIZE(SMALL, (LCSubsequence))
// metrics/lcsubstr.h
CHECK_MAX_SIZE(SMALL, (LCSubstr))
// metrics/levenshtein.h
CHECK_MAX_SIZE(SMALL, (Levenshtein))
// metrics/sorensen_dice.h
CHECK_MAX_SIZE(SMALL, (Sorensen_Dice))
// metrics/soundex_metric.h
// CHECK_MAX_SIZE(SMALL, (SoundexMetric))

// multivalue/geospatialrange.h
CHECK_MAX_SIZE(SMALL, (GeoSpatialRange))

// multivalue/keymaker.h
CHECK_MAX_SIZE(SMALL, (Multi_MultiValueKeyMaker))

// multivalue/range.h
CHECK_MAX_SIZE(SMALL, (MultipleValueRange))
CHECK_MAX_SIZE(SMALL, (MultipleValueGE))
CHECK_MAX_SIZE(SMALL, (MultipleValueLE))

// phonetic
CHECK_MAX_SIZE(SMALL, (SoundexEnglish))
CHECK_MAX_SIZE(SMALL, (SoundexFrench))
CHECK_MAX_SIZE(SMALL, (SoundexGerman))
CHECK_MAX_SIZE(SMALL, (SoundexSpanish))

// server/base_client.h
CHECK_MAX_SIZE(SMALL, (MetaBaseClient<DummyClient>))

// server/buffer.h
CHECK_MAX_SIZE(SMALL, (Buffer))

// server/http.h
CHECK_MAX_SIZE(SMALL, (Http))

// server/http_server.h
CHECK_MAX_SIZE(SMALL, (HttpServer))

// server/http_client.h
CHECK_MAX_SIZE(SMALL, (HttpClient))
CHECK_MAX_SIZE(SMALL, (Response))
CHECK_MAX_SIZE(SMALL, (Request))

// server/remote_protocol.h
CHECK_MAX_SIZE(SMALL, (RemoteProtocol))

// server/remote_protocol_server.h
CHECK_MAX_SIZE(SMALL, (RemoteProtocolServer))

// server/remote_protocol_client.h
CHECK_MAX_SIZE(SMALL, (RemoteProtocolClient))

// server/replication_protocol.h
CHECK_MAX_SIZE(SMALL, (ReplicationProtocol))

// server/replication_protocol_server.h
CHECK_MAX_SIZE(SMALL, (ReplicationProtocolServer))

// server/replication_protocol_client.h
CHECK_MAX_SIZE(SMALL, (ReplicationProtocolClient))

// server/discovery.h
CHECK_MAX_SIZE(SMALL, (Discovery))

#if XAPIAND_CHAISCRIPT
// chaipp/chaipp.h
CHECK_MAX_SIZE(SMALL, (chaipp::Processor))
#endif

}

#endif
