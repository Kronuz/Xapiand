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

// Stop-word lists (D137): the shipped lists are compiled in, a file under <data path>/stopwords overrides
// or adds a list, an unknown language is refused, and a list's fingerprint tells an index when it changed.
// The lists load once per process, so each scenario runs in a process of its own (`--scenario NAME`); with no
// argument the binary runs them all. `--probe MODE` measures what loading the lists costs.

#include "test_harness.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <sys/stat.h>
#include <unistd.h>

#include "database/schema.h"
#include "engine/text.h"
#include "exception_engine.h"
#include "msgpack.h"
#include "stopper.h"
#include "stopwords/builtin.h"

#if defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace {

// Live heap bytes, counted by the global allocation functions of this binary.
std::atomic<std::size_t> live_bytes{0};

constexpr std::size_t header = 16;

}  // namespace

void*
operator new(std::size_t size)
{
	auto* raw = static_cast<unsigned char*>(std::malloc(size + header));
	if (raw == nullptr) {
		throw std::bad_alloc();
	}
	std::memcpy(raw, &size, sizeof(size));
	live_bytes.fetch_add(size, std::memory_order_relaxed);
	return raw + header;
}

void
operator delete(void* ptr) noexcept
{
	if (ptr == nullptr) {
		return;
	}
	auto* raw = static_cast<unsigned char*>(ptr) - header;
	std::size_t size;
	std::memcpy(&size, raw, sizeof(size));
	live_bytes.fetch_sub(size, std::memory_order_relaxed);
	std::free(raw);
}

void
operator delete(void* ptr, std::size_t) noexcept
{
	operator delete(ptr);
}

namespace {

std::size_t
resident_kib()
{
#if defined(__APPLE__)
	mach_task_basic_info info;
	mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
	if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) != KERN_SUCCESS) {
		return 0;
	}
	return info.resident_size / 1024;
#elif defined(__linux__)
	std::size_t total = 0, resident = 0;
	if (std::FILE* f = std::fopen("/proc/self/statm", "r")) {
		if (std::fscanf(f, "%zu %zu", &total, &resident) != 2) {
			resident = 0;
		}
		std::fclose(f);
	}
	return resident * static_cast<std::size_t>(::sysconf(_SC_PAGESIZE)) / 1024;
#else
	return 0;
#endif
}

// One line per run: the mode, the microseconds the step took, the heap bytes and the resident KiB it added.
// Modes: `idle` (nothing), `preload` (the server's start-up call), `first` (the first lookup of LANG, after
// a preload), `cold` (the first lookup of LANG without a preload).
int
probe(const std::string& mode, const char* language)
{
	using clock = std::chrono::steady_clock;
	const auto heap0 = live_bytes.load();
	const auto rss0 = resident_kib();
	const auto t0 = clock::now();
	std::size_t words = 0;
	if (mode == "preload") {
		preloadStopWords();
	} else if (mode == "first" || mode == "cold") {
		if (mode == "first") {
			preloadStopWords();
		}
		const auto* stop = getStopWords(language);
		words = stop != nullptr && (*stop)("the") ? 1 : 0;
	}
	const auto us = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count() / 1000.0;
	std::printf("probe %s %s us=%.1f heap_bytes=%zu rss_kib=%zu stops_the=%zu\n", mode.c_str(), language, us,
		live_bytes.load() - heap0, resident_kib() - rss0, words);
	return 0;
}

namespace fs = std::filesystem;

// The shipped lists, as files of the source tree.
fs::path
shipped_dir()
{
	std::vector<fs::path> candidates{fs::path(__FILE__).parent_path().parent_path() / "stopwords"};
	for (auto dir = fs::current_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path()) {
		candidates.push_back(dir / "stopwords");
	}
	for (const auto& candidate : candidates) {
		if (fs::exists(candidate / "en.txt")) {
			return candidate;
		}
	}
	return candidates.front();
}

std::set<std::string>
words_of(const fs::path& file)
{
	std::ifstream in(file);
	return {std::istream_iterator<std::string>(in), std::istream_iterator<std::string>()};
}

// FNV-1a 64 of the sorted words, each followed by a NUL, as 16 hex digits: written out here, not shared with
// the code under test.
std::string
fingerprint_of(const std::set<std::string>& words)
{
	std::uint64_t hash = 14695981039346656037ull;
	for (const auto& word : words) {
		for (const char byte : word + std::string(1, '\0')) {
			hash = (hash ^ static_cast<unsigned char>(byte)) * 1099511628211ull;
		}
	}
	char text[17];
	std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(hash));
	return text;
}

bool
contains(const std::string& text, std::string_view part)
{
	return text.find(part) != std::string::npos;
}

// The what() of the exception `E` the call throws; "<none>" when it throws nothing, "<other>" otherwise.
template <class E, class F>
std::string
thrown(F&& call)
{
	try {
		call();
	} catch (const E& exc) {
		return exc.what();
	} catch (...) {
		return "<other>";
	}
	return "<none>";
}

// A fresh directory for the scenario, made the data path.
fs::path
data_path(const char* name)
{
	const auto root = fs::current_path() / (std::string("stopword_lists_") + name);
	fs::remove_all(root);
	fs::create_directories(root);
	::setenv("XAPIAND_DATA_PATH", root.c_str(), 1);
	return root;
}

void
write(const fs::path& file, const std::string& text)
{
	fs::create_directories(file.parent_path());
	std::ofstream(file) << text;
}

MsgPack
text_field(const char* language, const std::string& fingerprint)
{
	MsgPack field({{"_type", "text"}, {"_language", language}});
	if (!fingerprint.empty()) {
		field["_stop_fingerprint"] = fingerprint;
	}
	return field;
}

// A schema with a text field at the top, one inside an object and a field that has no language.
std::shared_ptr<Schema>
schema_of(const std::string& title_fingerprint, const std::string& body_fingerprint, const char* body_language = "fr")
{
	MsgPack props = MsgPack::MAP();
	props["title"] = text_field("en", title_fingerprint);
	props["plain"] = MsgPack({{"_type", "keyword"}});
	props["nested"] = MsgPack({{"_type", "object"}});
	props["nested"]["body"] = text_field(body_language, body_fingerprint);
	return std::make_shared<Schema>(std::make_shared<const MsgPack>(std::move(props)), nullptr, "");
}

// The fields of a schema, checked against the lists now in use: the index, the list and both fingerprints.
void
check_schema_stop_words(const std::string& en_now, const std::string& fr_now)
{
	// What the index recorded is still what the lists hold: let through.
	schema_of(en_now, fr_now)->check_stop_words("idx", false);

	// A field with another fingerprint recorded, at the top or inside an object, refuses the schema.
	const auto top = thrown<Error>([&] { schema_of("0123456789abcdef", fr_now)->check_stop_words("idx", false); });
	CHECK(contains(top, "idx"));
	CHECK(contains(top, "stop words list en"));
	CHECK(contains(top, "0123456789abcdef"));
	CHECK(contains(top, en_now));
	const auto inner = thrown<Error>([&] { schema_of(en_now, "fedcba9876543210")->check_stop_words("idx2", true); });
	CHECK(contains(inner, "idx2"));
	CHECK(contains(inner, "stop words list fr"));
	CHECK(contains(inner, "fedcba9876543210"));
	CHECK(contains(inner, fr_now));

	// A list that is not there is refused too.
	const auto gone = thrown<Error>([&] { schema_of(en_now, "fedcba9876543210", "gone")->check_stop_words("idx", false); });
	CHECK(contains(gone, "stop words list gone"));
	CHECK(contains(gone, "missing"));

	// A field with no record (indexed before the fingerprints) is let through. It adopts the current list's
	// fingerprint only when asked to, and then the schema is modified and keeps passing.
	auto old = schema_of("", "");
	old->check_stop_words("idx", false);
	CHECK(old->get_modified_schema() == nullptr);
	old->check_stop_words("idx", true);
	const auto adopted = old->get_modified_schema();
	REQUIRE(adopted != nullptr);
	CHECK_EQ(std::string(adopted->at("title").at("_stop_fingerprint").str_view()), en_now);
	CHECK_EQ(std::string(adopted->at("nested").at("body").at("_stop_fingerprint").str_view()), fr_now);
	CHECK(adopted->find("plain") != adopted->end());
	CHECK(adopted->at("plain").find("_stop_fingerprint") == adopted->at("plain").end());
	Schema(adopted, nullptr, "").check_stop_words("idx", false);
}

// A text field with a language records its list's fingerprint when the schema is written, and the readable
// schema does not show it.
void
check_schema_records(const std::string& en_now)
{
	Schema schema(Schema::get_initial_schema(), nullptr, "");
	schema.write(MsgPack({{"title", MsgPack({{"_type", "text"}, {"_language", "en"}})}}));
	const auto& title = schema.get_schema().at("title");
	REQUIRE(title.find("_stop_fingerprint") != title.end());
	CHECK_EQ(std::string(title.at("_stop_fingerprint").str_view()), en_now);
	CHECK_EQ(std::string(title.at("_language").str_view()), "en");
	CHECK(contains(schema.get_schema().to_string(), "_stop_fingerprint"));
	std::fprintf(stderr, "DEBUG %s\n", schema.to_string().c_str());
	CHECK(!contains(schema.to_string(), "_stop_fingerprint"));
	CHECK(contains(schema.to_string(), "_language"));
}

// With no stopwords directory every shipped language stops exactly the words of its file in the tree.
void
scenario_builtin()
{
	const auto root = data_path("builtin");   // nothing under it: no file can stand in for a list
	const auto dir = shipped_dir();
	std::size_t languages = 0;
	for (const auto& entry : fs::directory_iterator(dir)) {
		if (entry.path().extension() != ".txt") {
			continue;
		}
		const auto language = entry.path().stem().string();
		const auto words = words_of(entry.path());
		++languages;
		const auto* stop = getStopWords(language);
		REQUIRE(stop != nullptr);
		std::size_t missing = 0;
		for (const auto& word : words) {
			missing += !(*stop)(word);
		}
		CHECK_EQ(missing, std::size_t(0));
		const auto* listed = std::find_if(std::begin(stopwords::builtin), std::end(stopwords::builtin), [&](const auto& entry_) {
			return entry_.language == language;
		});
		REQUIRE(listed != std::end(stopwords::builtin));
		CHECK_EQ(listed->count, words.size());   // nothing beyond the file's words
		CHECK_EQ(stopWordsFingerprint(language), fingerprint_of(words));
		CHECK_FALSE((*stop)("xyzzy"));
		CHECK_FALSE((*stop)(""));
	}
	CHECK_EQ(languages, std::size_t(18));
	CHECK_EQ(languages, std::size(stopwords::builtin));

	const auto& en = *getStopWords("en");
	CHECK(en("the"));
	CHECK(en("a's"));
	CHECK_FALSE(en("dog"));
	CHECK(!(*getStopWords("fr"))("the"));
	CHECK((*getStopWords("fr"))("le"));
	CHECK(getStopWords("en") == getStopWords("en"));
	CHECK_EQ(stopWordsFingerprint("en"), "d1834faea3c14e55");

	// A language without a list is refused, in the lookup, the fingerprint and the query text.
	CHECK(contains(thrown<UnknownStopWordsError>([] { getStopWords("xx"); }), "xx"));
	CHECK_EQ(thrown<UnknownStopWordsError>([] { getStopWords("xx"); }), "Stop words list xx unknown");
	CHECK_EQ(thrown<UnknownStopWordsError>([] { stopWordsFingerprint("xx"); }), "Stop words list xx unknown");
	CHECK_EQ(thrown<UnknownStopWordsError>([] { getStopWords("EN"); }), "Stop words list EN unknown");
	CHECK_EQ(thrown<UnknownStopWordsError>([] { getStopWords(""); }), "Stop words list  unknown");

	engine::TextConfig config;
	config.stop_language = "xx";
	try {
		engine::parse_text("the dog", config);
		CHECK_TRUE(false);
	} catch (const InvalidTextConfigError& exc) {
		CHECK_EQ(exc.reason(), "Stop words list xx unknown");
	}
	std::string description;
	engine::parse_text("the dog", engine::TextConfig{}, &description);
	CHECK(contains(description, "the"));
	config.stop_language = "en";
	std::string stopped;
	engine::parse_text("the dog", config, &stopped);
	CHECK(!contains(stopped, "the"));
	CHECK(contains(stopped, "dog"));

	// An index that recorded the list's fingerprint is let through; any other fingerprint, or a list that
	// is not there, is refused with the index, the list and both fingerprints.
	checkStopWords("idx", "en", "d1834faea3c14e55");
	const auto changed = thrown<StopWordsMismatchError>([] { checkStopWords("idx", "en", "0123456789abcdef"); });
	CHECK(contains(changed, "idx"));
	CHECK(contains(changed, "stop words list en"));
	CHECK(contains(changed, "0123456789abcdef"));
	CHECK(contains(changed, "d1834faea3c14e55"));
	const auto gone = thrown<StopWordsMismatchError>([] { checkStopWords("idx", "gone", "0123456789abcdef"); });
	CHECK(contains(gone, "idx"));
	CHECK(contains(gone, "gone"));
	CHECK(contains(gone, "0123456789abcdef"));
	CHECK(contains(gone, "missing"));

	check_schema_stop_words("d1834faea3c14e55", stopWordsFingerprint("fr"));
	check_schema_records("d1834faea3c14e55");
	fs::remove_all(root);
}

// A file replaces its language's list; a custom name adds a list; the rest stay as shipped.
void
scenario_override()
{
	const auto root = data_path("override");
	write(root / "stopwords" / "en.txt", "zzz\nfoo zzz\n");
	write(root / "stopwords" / "xx.txt", "alpha beta\ngamma\n");
	write(root / "stopwords" / "notes.md", "dog\n");   // not a list
	std::string reversed;   // the shipped French words, backwards, each twice: the same list
	{
		const auto words = words_of(shipped_dir() / "fr.txt");
		for (auto it = words.rbegin(); it != words.rend(); ++it) {
			reversed += *it + "\n" + *it + "\n";
		}
	}
	write(root / "stopwords" / "fr.txt", reversed);
	preloadStopWords();
	fs::remove_all(root);   // the lists stay: nothing reads a file after the preload

	const auto& en = *getStopWords("en");
	CHECK(en("zzz"));
	CHECK(en("foo"));
	CHECK_FALSE(en("the"));   // the shipped words are gone
	CHECK_FALSE(en("dog"));
	CHECK_EQ(stopWordsFingerprint("en"), "ce679203ae8663b7");

	const auto& xx = *getStopWords("xx");
	CHECK(xx("alpha"));
	CHECK(xx("beta"));
	CHECK(xx("gamma"));
	CHECK_FALSE(xx("delta"));
	CHECK_EQ(stopWordsFingerprint("xx"), "344b9978a88eed3a");

	CHECK_EQ(thrown<UnknownStopWordsError>([] { getStopWords("notes"); }), "Stop words list notes unknown");
	CHECK_EQ(thrown<UnknownStopWordsError>([] { getStopWords("yy"); }), "Stop words list yy unknown");

	// An override with the shipped words has the shipped fingerprint, whatever its order and repeats.
	const auto words = words_of(shipped_dir() / "fr.txt");
	CHECK_EQ(stopWordsFingerprint("fr"), fingerprint_of(words));
	CHECK((*getStopWords("fr"))("le"));

	// A language with no file keeps the shipped list.
	CHECK((*getStopWords("de"))("und"));
	CHECK_EQ(stopWordsFingerprint("de"), fingerprint_of(words_of(shipped_dir() / "de.txt")));

	// An index built with the shipped English list is refused now that a file replaced it.
	const auto changed = thrown<StopWordsMismatchError>([] { checkStopWords("idx", "en", "d1834faea3c14e55"); });
	CHECK(contains(changed, "idx"));
	CHECK(contains(changed, "d1834faea3c14e55"));
	CHECK(contains(changed, "ce679203ae8663b7"));
	checkStopWords("idx", "en", "ce679203ae8663b7");
	checkStopWords("idx", "xx", "344b9978a88eed3a");
	checkStopWords("idx", "fr", fingerprint_of(words));
	const auto custom = thrown<StopWordsMismatchError>([] { checkStopWords("idx", "xx", "d1834faea3c14e55"); });
	CHECK(contains(custom, "344b9978a88eed3a"));

	// A schema recorded with the shipped English list is refused by the node whose file replaced it.
	const auto old_schema = thrown<Error>([] { schema_of("d1834faea3c14e55", stopWordsFingerprint("fr"))->check_stop_words("idx", false); });
	CHECK(contains(old_schema, "d1834faea3c14e55"));
	CHECK(contains(old_schema, "ce679203ae8663b7"));
	check_schema_stop_words("ce679203ae8663b7", stopWordsFingerprint("fr"));
	check_schema_records("ce679203ae8663b7");
}

// A file that cannot be read is an error, never a quiet return to the shipped list.
void
scenario_unreadable()
{
	const auto root = data_path("unreadable");
	write(root / "stopwords" / "en.txt", "zzz\n");
	::chmod((root / "stopwords" / "en.txt").c_str(), 0);
	if (std::ifstream(root / "stopwords" / "en.txt").is_open()) {
		std::printf("skip: the file is readable here (running as root)\n");
		return;
	}
	const auto error = thrown<std::runtime_error>([] { getStopWords("en"); });
	CHECK(contains(error, "Cannot open stop words file"));
	CHECK(contains(error, "en.txt"));
	CHECK_EQ(thrown<std::runtime_error>([] { getStopWords("de"); }), error);   // and so is every lookup
	::chmod((root / "stopwords" / "en.txt").c_str(), 0600);
	fs::remove_all(root);
}

// `--probe schema`: nanoseconds per Schema::check_stop_words over 200 text fields (a language each), which
// DatabaseHandler::get_schema runs on every schema it hands out: `cold` for a schema the thread has not
// verified (a walk), `warm` for one it has (a lookup by identity).
int
probe_schema()
{
	using clock = std::chrono::steady_clock;
	const char* const languages[] = {"en", "fr", "de", "es", "it"};
	MsgPack props = MsgPack::MAP();
	for (int i = 0; i < 200; ++i) {
		const auto* language = languages[i % 5];
		props["field" + std::to_string(i)] = text_field(language, stopWordsFingerprint(language));
	}
	constexpr int rounds = 2000;
	std::vector<std::unique_ptr<Schema>> schemas;   // one per schema version, as the schema cache hands them out
	for (int i = 0; i < rounds; ++i) {
		schemas.push_back(std::make_unique<Schema>(std::make_shared<const MsgPack>(props), nullptr, ""));
	}
	schemas[0]->check_stop_words("idx", false);   // builds the lists
	for (const char* name : {"cold", "warm"}) {
		const auto t0 = clock::now();
		for (const auto& schema : schemas) {
			schema->check_stop_words("idx", false);
		}
		const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(clock::now() - t0).count();
		std::printf("probe schema fields=200 %s ns_per_check=%.0f\n", name, static_cast<double>(ns) / rounds);
	}
	return 0;
}

struct Scenario {
	const char* name;
	void (*run)();
};

const Scenario scenarios[] = {
	{"builtin", scenario_builtin},
	{"override", scenario_override},
	{"unreadable", scenario_unreadable},
};

// Runs each scenario in a process of its own.
int
run_all(const char* self)
{
	int failed = 0;
	for (const auto& scenario : scenarios) {
		const std::string command = std::string("\"") + self + "\" --scenario " + scenario.name;
		const int status = std::system(command.c_str());
		std::fprintf(stderr, "%s %s\n", status == 0 ? "ok  " : "FAIL", scenario.name);
		failed += status != 0;
	}
	if (failed != 0) {
		std::fprintf(stderr, "%d of %zu scenarios FAILED\n", failed, std::size(scenarios));
		return 1;
	}
	std::printf("OK: %zu scenarios passed\n", std::size(scenarios));
	return 0;
}

}  // namespace

int
main(int argc, char** argv)
{
	if (argc > 2 && std::string(argv[1]) == "--scenario") {
		for (const auto& scenario : scenarios) {
			if (std::string(argv[2]) == scenario.name) {
				try {
					scenario.run();
				} catch (const std::exception& exc) {
					std::fprintf(stderr, "  FAIL %s: threw %s\n", scenario.name, exc.what());
					return 1;
				}
				if (testh::failures != 0) {
					std::fprintf(stderr, "%d of %d checks FAILED\n", testh::failures, testh::checks);
					return 1;
				}
				std::printf("OK: %d checks passed\n", testh::checks);
				return 0;
			}
		}
		std::fprintf(stderr, "no scenario %s\n", argv[2]);
		return 2;
	}
	if (argc > 2 && std::string(argv[1]) == "--probe" && std::string(argv[2]) == "schema") {
		return probe_schema();
	}
	if (argc > 2 && std::string(argv[1]) == "--probe") {
		return probe(argv[2], argc > 3 ? argv[3] : "en");
	}
	return run_all(argv[0]);
}
