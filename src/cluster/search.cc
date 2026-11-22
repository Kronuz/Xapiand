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

#include "cluster/search.h"

#include <exception>      // for std::exception_ptr, std::current_exception, std::rethrow_exception
#include <map>            // for std::map
#include <memory>         // for std::unique_ptr
#include <optional>       // for std::optional
#include <string_view>    // for std::string_view
#include <utility>        // for std::move

#include "engine/error.h"  // for Error, errc

namespace cluster {

namespace {

// The index of each shard of a part, for SearchRequest::index_of.
std::vector<std::uint32_t>
index_of(const std::vector<PartShard>& shards)
{
	std::vector<std::uint32_t> out;
	out.reserve(shards.size());
	for (const auto& shard : shards) {
		out.push_back(shard.index);
	}
	return out;
}

task::task<std::vector<engine::Shard*>>
held(LocalShards& local, const std::vector<PartShard>& shards)
{
	std::vector<engine::Shard*> engines;
	engines.reserve(shards.size());
	for (const auto& shard : shards) {
		auto* engine = co_await local.shard(shard.group, shard.incarnation);
		if (engine == nullptr) {
			throw engine::Error(engine::errc::not_found, "This node does not hold group " + std::to_string(shard.group) + " at incarnation " + std::to_string(shard.incarnation));
		}
		engines.push_back(engine);
	}
	co_return engines;
}

}  // namespace


task::task<std::vector<engine::IndexStatistics>>
serve_stats(LocalShards& local, const StatsPart& part)
{
	const auto engines = co_await held(local, part.shards);
	const auto indexes = index_of(part.shards);
	co_return co_await engine::gather(engines, *part.query, indexes);
}


task::task<PartAnswer>
serve_search(LocalShards& local, const SearchPart& part)
{
	const auto engines = co_await held(local, part.shards);
	const auto indexes = index_of(part.shards);
	engine::SearchRequest request = *part.request;
	request.index_of = indexes;
	std::unique_ptr<engine::MatchSpy> spy;
	if (part.request->spy != nullptr) {
		spy = part.request->spy->clone();
		request.spy = spy.get();
	}
	PartAnswer answer;
	answer.result = co_await engine::search(engines, request);
	if (spy) {
		answer.aggregations = spy->serialize_results();
	}
	co_return answer;
}


namespace {

// A shard of the search: where it is, and which of its replicas is asked.
struct Slot {
	std::string_view index;
	std::size_t shard = 0;
	PartShard part;
	std::vector<NodeId> order;   // replica_order()
	std::size_t at = 0;          // the replica asked now
	bool failed = false;
	NodeId last = 0;
	std::string reason;
};

std::string
describe(const std::exception_ptr& error)
{
	try {
		std::rethrow_exception(error);
	} catch (const engine::Error& e) {
		return std::string(engine::to_string(e.kind)) + ": " + e.reason();
	} catch (const std::exception& e) {
		return e.what();
	} catch (...) {
		return "unknown failure";
	}
}

// True for a failure that is the request's, not the replica's: the search ends with it.
bool
the_requests(const std::exception_ptr& error)
{
	try {
		std::rethrow_exception(error);
	} catch (const engine::Error& e) {
		return e.kind == engine::errc::syntax;
	} catch (...) {
		return false;
	}
}

template <class Answer>
struct Outcome {
	std::optional<Answer> answer;
	std::exception_ptr error;
};

template <class Answer>
task::task<Outcome<Answer>>
caught(task::task<Answer> ask)
{
	Outcome<Answer> outcome;
	try {
		outcome.answer = co_await std::move(ask);
	} catch (...) {
		outcome.error = std::current_exception();
	}
	co_return outcome;
}

// One round: each node is asked for the live slots it serves now, all at once; the slots of a node
// that failed move to their next replica and are asked once more, and fail for good after that.
// `ask(node, slots)` makes the ask, `take(node, slots, answer)` keeps an answer; a take that throws
// ends the search, since what it kept of the answer cannot be taken back.
template <class Answer, class Ask, class Take>
task::task<void>
round(std::vector<Slot>& slots, const Ask& ask, const Take& take)
{
	std::vector<std::size_t> pending;
	for (std::size_t i = 0; i < slots.size(); ++i) {
		if (!slots[i].failed) {
			pending.push_back(i);
		}
	}
	for (int attempt = 0; attempt < 2 && !pending.empty(); ++attempt) {
		std::map<NodeId, std::vector<std::size_t>> by_node;
		for (const auto i : pending) {
			by_node[slots[i].order[slots[i].at]].push_back(i);
		}
		std::vector<task::task<Outcome<Answer>>> asks;
		asks.reserve(by_node.size());
		for (const auto& [node, mine] : by_node) {
			asks.push_back(caught<Answer>(ask(node, mine)));
		}
		auto outcomes = co_await task::when_all(std::move(asks));
		pending.clear();
		std::size_t k = 0;
		for (const auto& [node, mine] : by_node) {
			auto& outcome = outcomes[k++];
			if (!outcome.error) {
				take(node, mine, std::move(*outcome.answer));
				continue;
			}
			if (the_requests(outcome.error)) {
				std::rethrow_exception(outcome.error);
			}
			const auto reason = describe(outcome.error);
			for (const auto i : mine) {
				auto& slot = slots[i];
				slot.last = node;
				slot.reason = reason;
				if (attempt == 0 && slot.at + 1 < slot.order.size()) {
					++slot.at;
					pending.push_back(i);
				} else {
					slot.failed = true;
				}
			}
		}
	}
}

std::vector<PartShard>
parts_of(const std::vector<Slot>& slots, const std::vector<std::size_t>& mine)
{
	std::vector<PartShard> shards;
	shards.reserve(mine.size());
	for (const auto i : mine) {
		shards.push_back(slots[i].part);
	}
	return shards;
}

}  // namespace


task::task<FanoutResult>
search(const ShardMapView& map, Nodes& nodes, std::span<const std::string> indexes, const engine::SearchRequest& request, const FanoutOptions& options)
{
	// The shards in the order one node numbers them: each index's in shard order, the indexes as named.
	std::vector<Slot> slots;
	for (std::size_t n = 0; n < indexes.size(); ++n) {
		const auto shards = co_await map.shards(indexes[n]);
		for (std::size_t s = 0; s < shards.size(); ++s) {
			Slot slot;
			slot.index = indexes[n];
			slot.shard = s;
			slot.part = {shards[s].group, shards[s].incarnation, slots.size(), static_cast<std::uint32_t>(n)};
			slot.order = replica_order(map, shards[s]);
			if (slot.order.empty()) {
				slot.failed = true;
				slot.reason = "no replica holds the shard";
			}
			slots.push_back(std::move(slot));
		}
	}

	FanoutResult out;
	out.total = slots.size();
	const auto account = [&](FanoutResult& result) {
		for (const auto& slot : slots) {
			if (slot.failed) {
				result.failed.push_back({std::string(slot.index), slot.shard, slot.last, slot.reason});
			}
		}
		result.successful = result.total - result.failed.size();
	};
	if (request.query == nullptr || request.query->empty() || slots.empty()) {
		account(out);
		co_return out;
	}

	// Statistics: one replica of each shard answers, and the sums by index weigh every part.
	std::vector<engine::IndexStatistics> sums(indexes.size());
	std::vector<bool> gathered(indexes.size(), false);
	const auto stats_ask = [&](NodeId node, const std::vector<std::size_t>& mine) {
		StatsPart part{request.query, parts_of(slots, mine), Clock::now() + options.attempt};
		return [](Nodes& nodes, NodeId node, StatsPart part) -> task::task<std::vector<engine::IndexStatistics>> {
			co_return co_await nodes.stats(node, part);
		}(nodes, node, std::move(part));
	};
	const auto stats_take = [&](NodeId, const std::vector<std::size_t>& mine, std::vector<engine::IndexStatistics> answer) {
		// One entry per index of the part, in its shards' order.
		std::vector<std::uint32_t> numbers;
		for (const auto i : mine) {
			if (numbers.empty() || numbers.back() != slots[i].part.index) {
				numbers.push_back(slots[i].part.index);
			}
		}
		if (answer.size() != numbers.size()) {
			throw engine::Error(engine::errc::internal, "A node answered statistics for other indexes");
		}
		for (std::size_t k = 0; k < numbers.size(); ++k) {
			sums[numbers[k]] += answer[k];
			gathered[numbers[k]] = true;
		}
	};
	co_await round<std::vector<engine::IndexStatistics>>(slots, stats_ask, stats_take);
	for (std::size_t n = 0; n < sums.size(); ++n) {
		if (gathered[n]) {
			engine::finish(*request.query, sums[n]);
		}
	}

	// The search: the same replicas, the merged statistics, the parts merged as one search's batches.
	engine::Merge merge(slots.size(), request);
	engine::SearchRequest part_request = merge.part();
	part_request.index_of = {};
	part_request.statistics = sums;
	const auto search_ask = [&](NodeId node, const std::vector<std::size_t>& mine) {
		SearchPart part{&part_request, parts_of(slots, mine), Clock::now() + options.attempt};
		return [](Nodes& nodes, NodeId node, SearchPart part) -> task::task<PartAnswer> {
			co_return co_await nodes.search(node, part);
		}(nodes, node, std::move(part));
	};
	const auto search_take = [&](NodeId, const std::vector<std::size_t>& mine, PartAnswer answer) {
		engine::BatchResult done;
		done.result = std::move(answer.result);
		done.positions.reserve(mine.size());
		for (const auto i : mine) {
			done.positions.push_back(slots[i].part.position);
		}
		merge.add(std::move(done));
		if (request.spy != nullptr) {
			request.spy->merge_results(answer.aggregations);
		}
	};
	co_await round<PartAnswer>(slots, search_ask, search_take);

	out.result = merge.finish();
	account(out);
	co_return out;
}

}  // namespace cluster
