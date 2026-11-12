/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// Times strata::Shard::view() (the published generation a read holds, released at once) per call, on one
// index, at 1 and 8 threads. Rounds alternate the thread counts and the handle modes, so drift in the
// host hits every row alike.
//
//   xapiand_bench_shard_view [--docs N] [--rounds R] [--batches B] [--batch K] [--threads a,b,...]
//
// A sample is the mean of one batch of K calls (a clock read is of the order of the call itself, so a
// single call is not timed). Rows: "shared" = every thread calls view() on one Shard object; "handles" =
// every thread opens its own read handle of the same index (handles of one index share its core).

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "engine/document.h"
#include "engine/open.h"
#include "engine/strata/shard.h"
#include "engine/sync.h"

namespace {

using Clock = std::chrono::steady_clock;

struct Config {
	int docs = 10000;
	int rounds = 15;
	int batches = 300;
	int batch = 1000;
	std::vector<int> threads{1, 8};
};

struct Row {
	std::string mode;
	int threads;
	std::vector<double> samples;       // ns per call, one per batch, all rounds
	std::vector<double> round_medians; // ns per call
	std::vector<double> round_rate;    // calls per second, all threads together
};

double percentile(std::vector<double> v, double p) {
	std::sort(v.begin(), v.end());
	return v[static_cast<std::size_t>(p * static_cast<double>(v.size() - 1) + 0.5)];
}

std::string db_path() {
	return (std::filesystem::current_path() / "bench_shard_view.db").string();
}

void build(const Config& cfg) {
	const auto path = db_path();
	std::filesystem::remove_all(path);
	auto opened = engine::sync_wait(engine::open_local(path, true, true));
	auto& shard = *opened.shard;
	for (int i = 1; i <= cfg.docs; ++i) {
		engine::Document doc;
		doc.set_key("Q" + std::to_string(i));
		doc.add_posting(doc.field("f"), "alpha", 1);
		doc.add_posting(doc.field("f"), "beta" + std::to_string(i % 97), 2);
		doc.add_value(3, "v" + std::to_string(i));
		doc.set_data("data " + std::to_string(i));
		engine::sync_wait(shard.add(doc, {}));
	}
	engine::sync_wait(shard.commit(engine::Transaction::none));
}

// One round: `threads` threads, each makes `batches` batches of `batch` calls; the samples are appended to row.
void round(const Config& cfg, Row& row, std::vector<engine::strata::Shard*>& shards, std::atomic<std::uint64_t>& sink) {
	const auto n = static_cast<std::size_t>(row.threads);
	std::vector<std::vector<double>> out(n);
	std::atomic<int> ready{0};
	std::atomic<bool> go{false};
	std::vector<std::thread> pool;
	for (std::size_t t = 0; t < n; ++t) {
		pool.emplace_back([&, t] {
			auto& shard = *shards[t];
			auto& mine = out[t];
			mine.reserve(static_cast<std::size_t>(cfg.batches));
			std::uint64_t seen = 0;
			++ready;
			while (!go.load(std::memory_order_acquire)) {
			}
			for (int b = 0; b < cfg.batches; ++b) {
				const auto start = Clock::now();
				for (int k = 0; k < cfg.batch; ++k) {
					auto viewed = shard.view();
					seen += viewed.segments().size() + 1;
				}
				const auto ns = std::chrono::duration<double, std::nano>(Clock::now() - start).count();
				mine.push_back(ns / cfg.batch);
			}
			sink += seen;
		});
	}
	while (ready.load() < row.threads) {
	}
	const auto start = Clock::now();
	go.store(true, std::memory_order_release);
	for (auto& th : pool) {
		th.join();
	}
	const auto secs = std::chrono::duration<double>(Clock::now() - start).count();
	std::vector<double> all;
	for (auto& mine : out) {
		all.insert(all.end(), mine.begin(), mine.end());
	}
	row.round_medians.push_back(percentile(all, 0.5));
	row.round_rate.push_back(static_cast<double>(n) * cfg.batches * cfg.batch / secs);
	row.samples.insert(row.samples.end(), all.begin(), all.end());
}

}  // namespace

int main(int argc, char** argv) {
	Config cfg;
	for (int i = 1; i + 1 < argc; i += 2) {
		const std::string flag = argv[i];
		const char* value = argv[i + 1];
		if (flag == "--docs") cfg.docs = std::atoi(value);
		else if (flag == "--rounds") cfg.rounds = std::atoi(value);
		else if (flag == "--batches") cfg.batches = std::atoi(value);
		else if (flag == "--batch") cfg.batch = std::atoi(value);
		else if (flag == "--threads") {
			cfg.threads.clear();
			for (const char* p = value; *p != '\0';) {
				cfg.threads.push_back(std::atoi(p));
				p = std::strchr(p, ',');
				if (p == nullptr) break;
				++p;
			}
		} else {
			std::fprintf(stderr, "unknown flag %s\n", flag.c_str());
			return 2;
		}
	}

	build(cfg);
	const auto path = db_path();
	auto first = engine::sync_wait(engine::open_local(path, false, false));
	auto* base = dynamic_cast<engine::strata::Shard*>(first.shard.get());
	if (base == nullptr || base->doc_count() != static_cast<engine::doccount>(cfg.docs)) {
		std::fprintf(stderr, "the index is not what the benchmark built\n");
		return 1;
	}
	const std::size_t most = static_cast<std::size_t>(*std::max_element(cfg.threads.begin(), cfg.threads.end()));
	std::vector<std::unique_ptr<engine::Shard>> own;
	std::vector<engine::strata::Shard*> separate;
	for (std::size_t t = 0; t < most; ++t) {
		own.push_back(engine::sync_wait(engine::open_local(path, false, false)).shard);
		separate.push_back(dynamic_cast<engine::strata::Shard*>(own.back().get()));
	}
	std::vector<engine::strata::Shard*> shared(most, base);

	std::vector<Row> rows;
	for (const char* mode : {"shared", "handles"}) {
		for (int t : cfg.threads) {
			rows.push_back(Row{mode, t, {}, {}, {}});
		}
	}
	std::atomic<std::uint64_t> sink{0};
	// A warm-up round per row, then the measured rounds, interleaved across rows.
	for (auto& row : rows) {
		auto& shards = row.mode == "shared" ? shared : separate;
		Row scratch{row.mode, row.threads, {}, {}, {}};
		round(cfg, scratch, shards, sink);
	}
	for (int r = 0; r < cfg.rounds; ++r) {
		for (auto& row : rows) {
			round(cfg, row, row.mode == "shared" ? shared : separate, sink);
		}
	}

	std::printf("docs=%d rounds=%d batches=%d batch=%d hw_threads=%u sink=%llu\n", cfg.docs, cfg.rounds, cfg.batches,
		cfg.batch, std::thread::hardware_concurrency(), static_cast<unsigned long long>(sink.load()));
	std::printf("%-8s %7s %12s %12s %14s %14s %14s\n", "mode", "threads", "median ns", "p90 ns", "round med min", "round med max",
		"Mcalls/s med");
	for (auto& row : rows) {
		std::printf("%-8s %7d %12.1f %12.1f %14.1f %14.1f %14.2f\n", row.mode.c_str(), row.threads, percentile(row.samples, 0.5),
			percentile(row.samples, 0.9), percentile(row.round_medians, 0.0), percentile(row.round_medians, 1.0),
			percentile(row.round_rate, 0.5) / 1e6);
	}
	std::filesystem::remove_all(path);
	return 0;
}
