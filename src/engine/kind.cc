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

#include "engine/kind.h"

#include <cstdlib>                 // for std::getenv
#include <filesystem>              // for std::filesystem
#include <mutex>                   // for std::mutex, std::lock_guard
#include <system_error>            // for std::error_code
#include <unordered_map>           // for std::unordered_map


namespace engine {

std::optional<Kind>
parse_kind(std::string_view name) noexcept
{
	if (name == "xapian") {
		return Kind::xapian;
	}
	if (name == "strata") {
		return Kind::strata;
	}
	return std::nullopt;
}


Kind
default_kind()
{
	static const Kind kind = [] {
		const char* env = std::getenv("XAPIAND_ENGINE");
		return (env ? parse_kind(env) : std::nullopt).value_or(Kind::xapian);
	}();
	return kind;
}


Kind
kind_of(const std::string& path)
{
	if (path.starts_with(".xapiand/") || path == ".xapiand") {
		return Kind::xapian;
	}

	// A shard's files do not change engine, so what was found once stays.
	static std::mutex mtx;
	static std::unordered_map<std::string, Kind> found;
	{
		std::lock_guard<std::mutex> lk(mtx);
		auto it = found.find(path);
		if (it != found.end()) {
			return it->second;
		}
	}

	namespace fs = std::filesystem;
	std::error_code ec;
	std::optional<Kind> kind;
	if (fs::exists(fs::path(path) / "strata", ec)) {
		kind = Kind::strata;
	} else if (fs::exists(fs::path(path) / "iamglass", ec)) {
		kind = Kind::xapian;
	}
	if (!kind) {
		return default_kind();
	}
	std::lock_guard<std::mutex> lk(mtx);
	found.emplace(path, *kind);
	return *kind;
}

}  // namespace engine
