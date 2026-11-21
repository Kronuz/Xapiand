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

#pragma once

#include <cstddef>                 // for std::size_t
#include <cstdint>                 // for std::uint64_t
#include <functional>              // for std::function
#include <optional>                // for std::optional
#include <vector>                  // for std::vector

#include "database/command.h"      // for Command, CommandWrite, COMMAND_ENTRY_BYTES
#include "engine/types.h"          // for engine::WriteResult
#include "prospect/index.hh"       // for strata::verdict
#include "task.hh"                 // for task::task


// RESTORE's batches (docs/design/cluster-release.md D17; contract xapiand:restore-batch). A shard's prepared writes,
// in input order, become commands of as many writes as fit one log entry; a write that alone exceeds it is a command
// of its own, which split_command chunks. Commands are pipelined: each is prepared before the ones ahead of it
// apply, so each write is an unconditional replace by key, and strata assigns its docid and version at apply.

class RestoreBatcher {
public:
	explicit RestoreBatcher(std::size_t limit = COMMAND_ENTRY_BYTES) : limit_(limit) { }

	// Adds the next prepared write, a replace by key at `schema_version`, without its expectation. Returns the
	// command it closed: the open one, when the write does not fit after its writes or the schema version changed.
	std::optional<Command> add(CommandWrite write, std::uint64_t schema_version);

	// Closes the open command, if it holds writes.
	std::optional<Command> flush();

private:
	std::size_t limit_;
	Command open_;
	std::size_t bytes_ = 0;   // the open command's writes, encoded
};

// The results of a command's writes from its accepted verdict: per write, its key and the docid and version
// strata assigned it.
std::vector<engine::WriteResult> restore_results(const Command& command, const ::strata::verdict& verdict);

// SEAM (journal-replaces-wal core step d): ShardEndpoint's propose path. It proposes a command (split_command past
// the entry limit) at the request's ack level, calls `placed` once the command holds its log position, and answers
// its accepted verdict, raising a refusal as the engine's error. RESTORE proposes each command once the one before
// it is placed, so the log holds them in input order, and awaits no apply but the last one's (D17).
using RestorePropose = std::function<task::task<::strata::verdict>(Command command, std::function<void()> placed)>;
