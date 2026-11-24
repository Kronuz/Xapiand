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

#include "database/restore_batch.h"

#include <algorithm>               // for std::count_if
#include <utility>                 // for std::move

#include "engine/error.h"          // for engine::Error, engine::errc
#include "varint.hh"               // for varint::leb128


namespace {

std::size_t
number_size(std::uint64_t value)
{
	return varint::leb128::size(value);
}

// A command entry's bytes: its kind and version, schema version and write count, then its writes.
std::size_t
entry_size(std::uint64_t schema_version, std::size_t count, std::size_t writes)
{
	return 2 + number_size(schema_version) + number_size(count) + writes;
}

}  // namespace


std::optional<Command>
RestoreBatcher::add(CommandWrite write, std::uint64_t schema_version)
{
	if (write.op != CommandOp::replace_key || write.key.empty()) {
		throw engine::Error(engine::errc::invalid, "A RESTORE write replaces a document by its key");
	}
	write.expected.reset();
	const auto size = encoded_size(write);
	std::optional<Command> closed;
	if (!open_.writes.empty() && (open_.schema_version != schema_version || entry_size(schema_version, open_.writes.size() + 1, bytes_ + size) > limit_)) {
		closed = flush();
	}
	open_.schema_version = schema_version;
	open_.writes.push_back(std::move(write));
	bytes_ += size;
	return closed;
}


std::optional<Command>
RestoreBatcher::flush()
{
	if (open_.writes.empty()) {
		return std::nullopt;
	}
	bytes_ = 0;
	return std::exchange(open_, Command{});
}


std::vector<engine::WriteResult>
restore_results(const Command& command, const ::strata::verdict& verdict)
{
	const auto documents = static_cast<std::size_t>(std::count_if(command.writes.begin(), command.writes.end(), [](const CommandWrite& write) {
		return write.op != CommandOp::set_metadata;
	}));
	if (verdict.result != ::strata::verdict::accepted || verdict.docids.size() != documents || verdict.versions.size() != documents) {
		throw engine::Error(engine::errc::internal, "A RESTORE batch's verdict does not answer its writes");
	}
	std::vector<engine::WriteResult> out;
	out.reserve(documents);
	for (const auto& write : command.writes) {
		if (write.op != CommandOp::set_metadata) {
			const auto i = out.size();
			out.push_back({static_cast<engine::docid>(verdict.docids[i]), verdict.versions[i], write.key});
		}
	}
	return out;
}
