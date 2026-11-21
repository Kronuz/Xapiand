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

// The call helpers the Lua tests share.

#include "support.h"

namespace detail {

Outcome outcome_of(std::exception_ptr error) {
	Outcome out;
	if (!error) {
		return out;
	}
	try {
		std::rethrow_exception(error);
	} catch (const lua::ScriptLimitError& e) {
		out.limit = true;
		out.what = e.what();
	} catch (const lua::ScriptResultTooLarge& e) {
		out.too_large = true;
		out.what = e.what();
	} catch (const lua::ScriptError& e) {
		out.script_error = true;
		out.what = e.what();
	} catch (const task::cancelled&) {
		out.cancelled = true;
	} catch (const std::exception& e) {
		out.failed = true;
		out.what = e.what();
	}
	return out;
}

MsgPack doc_of(std::initializer_list<std::pair<const char*, int64_t>> fields) {
	MsgPack doc = MsgPack::MAP();
	for (auto& [key, value] : fields) {
		doc[key] = value;
	}
	return doc;
}

// Runs one call to its end on this thread (no budget: it never suspends).
Outcome call(Processor& processor, MsgPack doc, const MsgPack& params) {
	::alarm(600);   // the watchdog bounds one call, not the whole run (a sanitizer build is slow)
	Outcome out;
	out.doc = std::move(doc);
	MsgPack old_doc = MsgPack::MAP();
	std::exception_ptr error;
	try {
		task::sync_wait(processor("PUT", out.doc, old_doc, params));
	} catch (...) {
		error = std::current_exception();
	}
	auto result = outcome_of(error);
	result.doc = std::move(out.doc);
	return result;
}

Outcome call(const std::string& body, MsgPack doc, Limits limits) {
	Processor processor("test", body, MsgPack::MAP(), limits);
	return call(processor, std::move(doc));
}

void start(task::inline_scheduler& scheduler, Pending& pending, Processor& processor, task::budget& budget) {
	task::spawn(task::scheduler_ref::of(scheduler),
		processor("PUT", pending.doc, pending.old_doc, pending.params, &budget),
		[&pending](std::exception_ptr error) {
			pending.outcome = outcome_of(error);
			pending.finished = true;
		});
}

// A nil assigned to a field leaves no field.
bool absent(const MsgPack& doc, const char* key) {
	return doc.find(key) == doc.end();
}

std::string str(const MsgPack& doc, const char* key) {
	return doc.at(key).str();
}

Limits quick() {
	Limits limits;
	limits.max_instructions = 2'000'000;
	limits.max_bytes = 8u << 20;
	return limits;
}

// Other work on the loop: `rounds` turns, then it reports whether the script had finished.
task::task<void> other_requests(std::size_t rounds, std::size_t& turns, const bool& script_finished, bool& script_finished_at_end) {
	for (; turns < rounds; ++turns) {
		co_await task::yield();
	}
	script_finished_at_end = script_finished;
}

// About 800,000 short strings in 800 tables, alive at the call's end, and a print once they are built.
extern const std::string short_strings_800k =
	"local t = {} for j = 1, 800 do local u = {} for k = 1, 1000 do u[k] = j .. ':' .. k end t[j] = u end print('built') ";

}  // namespace detail
