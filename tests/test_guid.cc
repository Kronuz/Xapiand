/*
 * Copyright (C) 2015-2018 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "test_guid.h"

#include "../src/base_x.hh"
#include "../src/guid/guid.h"
#include "utils.h"

#define B59 (Base59::dubaluchk())


constexpr int NUM_TESTS = 1000;

constexpr size_t MIN_COMPACTED_LENGTH =  2;
constexpr size_t MAX_COMPACTED_LENGTH = 11;
constexpr size_t MIN_CONDENSED_LENGTH =  2;
constexpr size_t MAX_CONDENSED_LENGTH = 16;
constexpr size_t MIN_EXPANDED_LENGTH  =  3;
constexpr size_t MAX_EXPANDED_LENGTH  = 17;


int test_generator_guid() {
	INIT_LOG
	GuidGenerator generator;

	int cont = 0;

	auto g1 = generator.newGuid(false);
	auto g2 = generator.newGuid(false);
	auto g3 = generator.newGuid(false);

	L_DEBUG("Guids generated: %s  %s  %s", repr(g1.to_string()).c_str(), repr(g2.to_string()).c_str(), repr(g3.to_string()).c_str());
	if (g1 == g2 || g1 == g3 || g2 == g3) {
		L_ERR("ERROR: Not all random guids are different");
		++cont;
	}

	auto g4 = generator.newGuid();
	auto g5 = generator.newGuid();
	auto g6 = generator.newGuid();

	L_DEBUG("Guids generated: %s  %s  %s", repr(g4.to_string()).c_str(), repr(g5.to_string()).c_str(), repr(g6.to_string()).c_str());
	if (g4 == g5 || g4 == g6 || g5 == g6) {
		L_ERR("ERROR: Not all random guids are different");
		++cont;
	}

	RETURN(cont);
}


int test_constructor_guid() {
	int cont = 0;

	std::string u1("3c0f2be3-ff4f-40ab-b157-c51a81eff176");
	std::string u2("e47fcfdf-8db6-4469-a97f-57146dc41ced");
	std::string u3("b2ce58e8-d049-4705-b0cb-fe7435843781");

	Guid s1(u1);
	Guid s2(u2);
	Guid s3(u3);
	Guid s4(u1);

	if (s1 == s2) {
		L_ERR("ERROR: s1 and s2 must be different");
		++cont;
	}

	if (s1 != s4) {
		L_ERR("ERROR: s1 and s4 must be equal");
		++cont;
	}

	if (s1.to_string() != u1) {
		L_ERR("ERROR: string generated from s1 is wrong");
		++cont;
	}

	if (s2.to_string() != u2) {
		L_ERR("ERROR: string generated from s2 is wrong");
		++cont;
	}

	if (s3.to_string() != u3) {
		L_ERR("ERROR: string generated from s3 is wrong");
		++cont;
	}

	RETURN(cont);
}


int test_special_guids() {
	std::vector<std::string> special_uuids({
		"00000000-0000-0000-0000-000000000000",
		"00000000-0000-1000-8000-000000000000",
		"00000000-0000-1000-a000-000000000000",
		"00000000-0000-4000-b000-000000000000",
		"00000000-2000-1000-c000-000000000000",
		"00000000-2000-4000-c000-000000000000",
		"00000000-2000-2000-0000-000000000000",
	});

	int cont = 0;
	for (const auto& uuid_orig : special_uuids) {
		Guid guid(uuid_orig);
		Guid guid2 = Guid::unserialise(guid.serialise());
		const auto uuid_rec = guid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\tResult: %s\n\tExpected: %s", uuid_rec.c_str(), uuid_orig.c_str());
		}
	}

	RETURN(cont);
}


int test_compacted_guids() {
	int cont = 0;
	size_t min_length = 20, max_length = 0;
	for (int i = 0; i < NUM_TESTS; ++i) {
		Guid guid = GuidGenerator().newGuid(true);
		const auto uuid_orig = guid.to_string();
		const auto serialised = guid.serialise();
		Guid guid2 = Guid::unserialise(serialised);
		const auto uuid_rec = guid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\tResult: %s\n\tExpected: %s", uuid_rec.c_str(), uuid_orig.c_str());
		}
		if (max_length < serialised.length()) {
			max_length = serialised.length();
		}
		if (min_length > serialised.length()) {
			min_length = serialised.length();
		}
	}

	if (max_length > MAX_COMPACTED_LENGTH) {
		L_ERR("ERROR: Max length for compacted uuid is %zu", MAX_COMPACTED_LENGTH);
		++cont;
	}

	if (min_length < MIN_COMPACTED_LENGTH) {
		L_ERR("ERROR: Min length for compacted uuid is %zu", MIN_COMPACTED_LENGTH);
		++cont;
	}

	RETURN(cont);
}


int test_condensed_guids() {
	int cont = 0;
	size_t min_length = 20, max_length = 0;
	for (int i = 0; i < NUM_TESTS; ++i) {
		Guid guid = GuidGenerator().newGuid(false);
		const auto uuid_orig = guid.to_string();
		const auto serialised = guid.serialise();
		Guid guid2 = Guid::unserialise(serialised);
		const auto uuid_rec = guid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\tResult: %s\n\tExpected: %s", uuid_rec.c_str(), uuid_orig.c_str());
		}
		if (max_length < serialised.length()) {
			max_length = serialised.length();
		}
		if (min_length > serialised.length()) {
			min_length = serialised.length();
		}
	}

	if (max_length > MAX_CONDENSED_LENGTH) {
		L_ERR("ERROR: Max length for condensed uuid is %zu", MAX_CONDENSED_LENGTH);
		++cont;
	}

	if (min_length < MIN_CONDENSED_LENGTH) {
		L_ERR("ERROR: Min length for condensed uuid is %zu", MIN_CONDENSED_LENGTH);
		++cont;
	}

	RETURN(cont);
}


int test_expanded_guids() {
	int cont = 0;
	size_t min_length = 20, max_length = 0;
	for (auto i = 0; i < NUM_TESTS; ++i) {
		std::string uuid_orig;
		uuid_orig.reserve(36);
		const char x[] = {
			'0', '1', '2',  '3',  '4',  '5',  '6',  '7',
			'8', '9', 'a',  'b',  'c',  'd',  'e',  'f',
		};
		for (int j = 0; j < 8; ++j) {
			uuid_orig.push_back(x[random_int(0, 15)]);
		}
		uuid_orig.push_back('-');
		for (int j = 0; j < 4; ++j) {
			uuid_orig.push_back(x[random_int(0, 15)]);
		}
		uuid_orig.push_back('-');
		for (int j = 0; j < 4; ++j) {
			uuid_orig.push_back(x[random_int(0, 15)]);
		}
		uuid_orig.push_back('-');
		for (int j = 0; j < 4; ++j) {
			uuid_orig.push_back(x[random_int(0, 15)]);
		}
		uuid_orig.push_back('-');
		for (int j = 0; j < 12; ++j) {
			uuid_orig.push_back(x[random_int(0, 15)]);
		}
		// If random uuid is rfc 4122, change the variant.
		const auto& version = uuid_orig[14];
		auto& variant = uuid_orig[19];
		if ((version == 1 || version == 4) && (variant == '8' || variant == '9' || variant == 'a' || variant == 'b')) {
			variant = '7';
		}
		Guid guid(uuid_orig);
		const auto serialised = guid.serialise();
		Guid guid2 = Guid::unserialise(serialised);
		const auto uuid_rec = guid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\tResult: %s\n\tExpected: %s", uuid_rec.c_str(), uuid_orig.c_str());
		}
		if (max_length < serialised.length()) {
			max_length = serialised.length();
		}
		if (min_length > serialised.length()) {
			min_length = serialised.length();
		}
	}

	if (max_length > MAX_EXPANDED_LENGTH) {
		L_ERR("ERROR: Max length for expanded uuid is %zu", MAX_EXPANDED_LENGTH);
		++cont;
	}

	if (min_length < MIN_EXPANDED_LENGTH) {
		L_ERR("ERROR: Min length for expanded uuid is %zu", MIN_EXPANDED_LENGTH);
		++cont;
	}

	RETURN(cont);
}


int test_several_guids() {
	size_t cont = 0;
	for (auto i = 0; i < NUM_TESTS; ++i) {
		std::vector<std::string> str_uuids;
		std::vector<std::string> norm_uuids;
		switch (i % 3) {
			case 0: {
				Guid guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(guid.to_string());

				guid = GuidGenerator().newGuid(false);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(guid.to_string());

				guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(guid.to_string());

				guid = GuidGenerator().newGuid(false);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(guid.to_string());

				guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(guid.to_string());
				break;
			}
			case 1: {
				Guid guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(B59.encode(guid.serialise()));

				guid = GuidGenerator().newGuid(false);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(B59.encode(guid.serialise()));

				guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(B59.encode(guid.serialise()));

				guid = GuidGenerator().newGuid(false);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(B59.encode(guid.serialise()));

				guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				norm_uuids.push_back(B59.encode(guid.serialise()));
				break;
			}
			default: {
				Guid guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				auto serialised = guid.serialise();

				guid = GuidGenerator().newGuid(false);
				str_uuids.push_back(guid.to_string());
				serialised.append(guid.serialise());

				guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				serialised.append(guid.serialise());

				guid = GuidGenerator().newGuid(false);
				str_uuids.push_back(guid.to_string());
				serialised.append(guid.serialise());

				guid = GuidGenerator().newGuid(true);
				str_uuids.push_back(guid.to_string());
				serialised.append(guid.serialise());

				norm_uuids.push_back(B59.encode(serialised));
				break;
			}
		}

		std::string guids_serialised;
		for (auto& encoded : norm_uuids) {
			std::string decoded;
			try {
				B59.decode(decoded, encoded);
				if (Guid::is_serialised(decoded)) {
					guids_serialised.append(decoded);
					continue;
				}
			} catch (const std::invalid_argument&) { }
			try {
				Guid guid(encoded);
				guids_serialised.append(guid.serialise());
				continue;
			} catch (const std::invalid_argument&) { }
			L_ERR("Invalid encoded UUID format in: %s", encoded.c_str());
		}

		std::string str_uuids_serialised;
		for (const auto& s : str_uuids) {
			str_uuids_serialised.append(Guid(s).serialise());
		}

		std::vector<Guid> guids;
		Guid::unserialise(guids_serialised, std::back_inserter(guids));
		if (guids.size() != str_uuids.size()) {
			++cont;
			L_ERR("ERROR: Different sizes: %zu != %zu\n\tResult: %s\n\tExpected: %s", guids.size(), str_uuids.size(), repr(guids_serialised).c_str(), repr(str_uuids_serialised).c_str());
		} else {
			auto it = str_uuids.begin();
			for (const auto& guid : guids) {
				const auto str_guid = guid.to_string();
				if (str_guid != *it) {
					++cont;
					L_ERR("ERROR:\n\tResult: %s\n\tExpected: %s", str_guid.c_str(), it->c_str());
				}
				++it;
			}
		}
	}

	RETURN(cont);
}
