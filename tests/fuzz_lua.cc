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

// Fuzzes the Lua script runner: random scripts (statements that write globals and the libraries,
// sort, match, build tables, loop, catch errors), byte-mutated, run under small bounds. The property:
// three calls of one script on three documents, interleaved on one processor in tiny slices, end
// exactly as the same three calls run one after another on fresh processors, and no call crashes,
// hangs or leaves the processor unable to serve.
//
//   xapiand_fuzz_lua SECONDS [SEED]
//
// Exits non-zero on the first violation. Under -fsanitize it finds the memory errors.

#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#include <unistd.h>

#include "lua/exception.h"
#include "lua/processor.h"
#include "task.hh"

namespace {

struct Rng {
	std::uint64_t s;
	std::uint64_t next() {
		s += 0x9e3779b97f4a7c15ULL;
		std::uint64_t z = s;
		z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
		z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
		return z ^ (z >> 31);
	}
	std::size_t below(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(next() % n); }
};

std::string num(Rng& rng) {
	static const char* const pool[] = {"0", "1", "2", "3", "7", "10", "100", "1000", "20000", "-1", "2^53", "0.5"};
	return pool[rng.below(sizeof pool / sizeof *pool)];
}

std::string statement(Rng& rng) {
	const std::string n = num(rng), m = num(rng);
	switch (rng.below(36)) {
		case 0: return "_doc.a = (_doc.a or 0) + " + n;
		case 1: return "leak = (leak or 0) + 1; _doc.g = leak";
		case 2: return "string.x = (string.x or 0) + 1; _doc.s = string.x";
		case 3: return "for i = 1, " + n + " do _doc.a = (_doc.a or 0) + i end";
		case 4: return "local t = {} for i = 1, " + n + " do t[i] = (i * 7919) % 1003 end table.sort(t, function(x, y) return x < y end) _doc.t = t[1]";
		case 5: return "pcall(function() error('e') end)";
		case 6: return "_doc.m = string.rep('ab', " + n + "):gsub('a', 'c')";
		case 7: return "_doc.f = string.find(string.rep('a', " + n + "), 'a*b')";
		case 8: return "while true do end";
		case 9: return "string.gsub('xyz', '.', function(c) _doc.c = (_doc.c or '') .. c end)";
		case 10: return "_doc.l = #(_doc.list or {}); _doc.list = {1, 2, 3}";
		case 11: return "_doc.nested = {a = {b = {c = " + n + "}}}";
		case 12: return "_doc.cyc = _doc";
		case 13: return "table.leak = " + n + "; _doc.tl = table.leak";
		case 14: return "_doc.u = utf8.len('h\\u{e9}llo') + math.floor(" + n + ")";
		case 15: return "local s = 0 for k, v in pairs(_doc) do s = s + 1 end _doc.n = s";
		case 16: return "_doc.k = tostring(_G == _ENV) .. type(getmetatable(_G))";
		case 17: return "pcall(string.rep, 'x', 1 << 40)";
		case 18: return "local t = {" + n + ", " + m + ", 3} table.sort(t) _doc.so = t[1]";
		case 19: return "_doc.p = string.match('2026-11-01', '(%d+)-(%d+)') or 'none'";
		case 20: return "error('script error')";
		case 21: return "setmetatable(_doc, {__index = function() return 1 end}); _doc.x = _doc.nope";
		case 22: return "_doc.tx = select('#', table.unpack({1, 2, 3}))";
		case 23: return "do local x <const> = " + n + "; _doc.cx = x end";
		case 24: return "local b = string.rep('ab', " + n + ") _doc.up = #b:upper() + #b:reverse() + #b:sub(2, -2) + (b:find('ba', 1, true) or 0)";
		case 25: return "local h = string.rep('a', " + n + ") _doc.pf = h:find(string.rep('a', " + m + ") .. 'b', 1, true) or 0";
		case 26: return "local t = {} for i = 1, " + n + " do t[i] = 'x' .. i end _doc.cc = #table.concat(t, ',') + select('#', table.unpack(t, 1, 100))";
		case 27: return "local t = {1, 2, 3} for i = 1, " + n + " do table.insert(t, 1, i) end table.remove(t, 1) table.move(t, 1, 3, 2) _doc.ti = #t";
		case 28: return "local function f(...) if select('#', ...) > " + n + " then return ... end return f(1, ...) end _doc.va = select('#', f())";
		case 29: return "_doc.ut = (utf8.len(string.rep('h\\u{e9}', " + n + ")) or 0) + #utf8.char(233, 8364) + (utf8.codepoint('h\\u{e9}', 1, -1))";
		case 30: return "pcall(setmetatable, {}, {__close = print}) pcall(setmetatable, {}, {__mode = 'k'}) _doc.wk = 1";
		case 31: return "local mt = {} local t = setmetatable({}, mt) mt.__close = function() error('x') end pcall(function() local c <close> = t end)";
		case 32: return "local s = string.rep('x', " + n + ") local t = {} for i = 1, 8 do t[i] = s end _doc.big = t";
		case 33: return "local o = {1, 2} local t = {} for i = 1, " + n + " do t[i] = o end _doc.sh = t";
		case 34: return "print('p', " + n + ", string.rep('q', 3000))";
		default: return "_doc.fmt = string.format('%s|%q|%5.1f', string.rep('z', " + n + "), 'a\\nb', " + n + ")";
	}
}

std::string script(Rng& rng) {
	std::string out;
	for (auto n = 1 + rng.below(6); n > 0; --n) {
		out += statement(rng);
		out += rng.below(2) ? "; " : "\n";
	}
	return out;
}

std::string mutate(Rng& rng, std::string text) {
	for (auto edits = rng.below(3); edits > 0 && !text.empty(); --edits) {
		const auto at = rng.below(text.size());
		switch (rng.below(3)) {
			case 0: text[at] = static_cast<char>(32 + rng.below(95)); break;
			case 1: text.erase(at, 1 + rng.below(8)); break;
			default: text.insert(at, text.substr(at, 1 + rng.below(8))); break;
		}
	}
	return text;
}

lua::Limits limits() {
	lua::Limits l;
	l.max_instructions = 300'000;
	l.max_unyielded_hooks = 20;
	l.max_bytes = 256u << 20;   // far above anything the statements build, so interleaving cannot trip it
	return l;
}

struct Result {
	std::string kind;   // ok, limit, error
	std::string doc;
	bool operator==(const Result& o) const { return kind == o.kind && doc == o.doc; }
};

// A document as text with its map keys sorted: the order of a Lua table's fields is not part of its value.
std::string canon(const MsgPack& value) {
	if (value.is_map()) {
		std::vector<std::string> fields;
		for (auto it = value.begin(); it != value.end(); ++it) {
			fields.push_back(it->str() + ":" + canon(it.value()));
		}
		std::sort(fields.begin(), fields.end());
		std::string out = "{";
		for (auto& field : fields) {
			out += field + ",";
		}
		return out + "}";
	}
	if (value.is_array()) {
		std::string out = "[";
		for (auto it = value.begin(); it != value.end(); ++it) {
			out += canon(it.value()) + ",";
		}
		return out + "]";
	}
	return value.to_string();
}

std::string brief(const std::string& text) {
	return text.size() > 300 ? text.substr(0, 300) + "..." : text;
}

Result classify(std::exception_ptr error, const MsgPack& doc) {
	if (!error) {
		return {"ok", canon(doc)};
	}
	try {
		std::rethrow_exception(error);
	} catch (const lua::ScriptLimitError&) {
		return {"limit", ""};
	} catch (const lua::ScriptResultTooLarge&) {
		return {"too large", ""};
	} catch (const std::exception&) {
		return {"error", ""};
	}
	return {"?", ""};
}

std::string current_script;

void show_current() {
	std::fprintf(stderr, "script:\n%s\n", current_script.c_str());
}

struct Pending {
	MsgPack doc, old_doc = MsgPack::MAP(), params = MsgPack::MAP();
	bool finished = false;
	std::exception_ptr error;
};

}  // namespace

int main(int argc, char** argv) {
	const double seconds = argc > 1 ? std::atof(argv[1]) : 10;
	Rng rng{argc > 2 ? std::strtoull(argv[2], nullptr, 10) : 1};
	const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<long>(seconds * 1000));
	std::size_t runs = 0, refused = 0, limited = 0;
	std::signal(SIGALRM, [](int) { std::fprintf(stderr, "HANG: "); show_current(); ::_exit(3); });
	while (std::chrono::steady_clock::now() < end) {
		auto body = script(rng);
		if (rng.below(3) == 0) {
			body = mutate(rng, std::move(body));
		}
		current_script = body;
		::alarm(60);
		++runs;
		std::vector<MsgPack> docs;
		for (int i = 0; i < 3; ++i) {
			docs.push_back(MsgPack({{"id", i}, {"a", static_cast<int64_t>(rng.below(5))}}));
		}

		std::vector<Result> sequential;
		bool built = true;
		for (int i = 0; i < 3 && built; ++i) {
			try {
				lua::Processor processor("fuzz", body, MsgPack::MAP(), limits());
				MsgPack doc = docs[i], old_doc = MsgPack::MAP();
				std::exception_ptr error;
				try {
					task::sync_wait(processor("PUT", doc, old_doc, MsgPack::MAP()));
				} catch (...) {
					error = std::current_exception();
				}
				sequential.push_back(classify(error, doc));
			} catch (const std::exception&) {
				built = false;   // a syntax error: refused at compile
			}
		}
		if (!built) {
			++refused;
			::alarm(0);
			continue;
		}

		lua::Processor shared("fuzz", body, MsgPack::MAP(), limits());
		task::inline_scheduler scheduler;
		task::budget budget(std::chrono::nanoseconds(1));
		Pending pending[3];
		for (int i = 0; i < 3; ++i) {
			pending[i].doc = docs[i];
			task::spawn(task::scheduler_ref::of(scheduler), shared("PUT", pending[i].doc, pending[i].old_doc, pending[i].params, &budget),
				[&p = pending[i]](std::exception_ptr e) { p.error = e; p.finished = true; });
		}
		scheduler.run();
		for (int i = 0; i < 3; ++i) {
			if (!pending[i].finished) {
				std::printf("call %d never finished\n", i);
				show_current();
				return 1;
			}
			auto interleaved = classify(pending[i].error, pending[i].doc);
			if (!(interleaved == sequential[i])) {
				std::printf("call %d differs: sequential %s %s, interleaved %s %s\n", i, sequential[i].kind.c_str(), brief(sequential[i].doc).c_str(),
					interleaved.kind.c_str(), brief(interleaved.doc).c_str());
				show_current();
				return 1;
			}
			if (interleaved.kind == "limit") {
				++limited;
			}
		}
		// the processor still serves
		MsgPack doc = MsgPack::MAP(), old_doc = MsgPack::MAP();
		try {
			task::sync_wait(shared("PUT", doc, old_doc, MsgPack::MAP()));
		} catch (const std::exception&) {
		}
		::alarm(0);
	}
	std::printf("%zu scripts, %zu refused at compile, %zu calls ended at a bound\n", runs, refused, limited);
	return 0;
}
