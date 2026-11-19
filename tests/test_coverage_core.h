// Value checks for the core coverage tests: a mismatch prints both sides.

#pragma once

#include <cstdio>
#include <string>
#include <string_view>

#include "test_harness.h"

namespace covcore {

inline std::string show(const std::string& v) { return "\"" + v + "\""; }
inline std::string show(std::string_view v) { return "\"" + std::string(v) + "\""; }
inline std::string show(const char* v) { return "\"" + std::string(v) + "\""; }
inline std::string show(bool v) { return v ? "true" : "false"; }
template <typename T>
inline std::string show(const T& v) { return std::to_string(v); }

}  // namespace covcore

// CHECK that a == b, printing both values when they differ.
#define SEE(a, b) \
	do { \
		++testh::checks; \
		const auto got_ = (a); \
		const auto want_ = (b); \
		if (!(got_ == want_)) { \
			std::fprintf(stderr, "  FAIL %s:%d: %s\n    got  %s\n    want %s\n", __FILE__, __LINE__, #a, covcore::show(got_).c_str(), covcore::show(want_).c_str()); \
			++testh::failures; \
		} \
	} while (0)

// The message of the exception `expr` throws as `exc`, or "<none>" when it throws nothing or another type.
#define THROWN(expr, exc) \
	([&]() -> std::string { \
		try { (void)(expr); } catch (const exc& e_) { return e_.what(); } catch (...) { return "<other>"; } \
		return "<none>"; \
	}())

#define CHECK_THROWS_WITH(expr, exc, text) \
	do { \
		++testh::checks; \
		const std::string msg_ = THROWN(expr, exc); \
		if (msg_.find(text) == std::string::npos) { \
			std::fprintf(stderr, "  FAIL %s:%d: %s\n    threw %s\n    want it to hold %s\n", __FILE__, __LINE__, #expr, covcore::show(msg_).c_str(), covcore::show(std::string(text)).c_str()); \
			++testh::failures; \
		} \
	} while (0)
