/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "test_uuid.h"

#include <unordered_set>

#include "../src/base_x.hh"
#include "../src/cuuid/uuid.h"
#include "../src/random.hh"
#include "utils.h"

#define B59 (Base59::dubaluchk())

constexpr int NUM_TESTS = 1000;

constexpr size_t MIN_COMPACTED_LENGTH =  2;
constexpr size_t MAX_COMPACTED_LENGTH = 11;
constexpr size_t MIN_CONDENSED_LENGTH =  2;
constexpr size_t MAX_CONDENSED_LENGTH = 16;
constexpr size_t MIN_EXPANDED_LENGTH  =  3;
constexpr size_t MAX_EXPANDED_LENGTH  = 17;


int test_generator_uuid(bool compact) {
	INIT_LOG

	UUIDGenerator generator;

	int cont = 0;

	auto g1 = generator(compact);
	auto g2 = generator(compact);
	auto g3 = generator(compact);
	L_DEBUG("UUIDs generated: {}  {}  {}", repr(g1.to_string()), repr(g2.to_string()), repr(g3.to_string()));
	if (g1 == g2 || g1 == g3 || g2 == g3) {
		L_ERR("ERROR: Not all random UUIDs are different");
		++cont;
	}

	std::unordered_set<std::string> uuids;
	for (int i = 0; i < NUM_TESTS; ++i) {
		uuids.insert(generator(compact).serialise());
	}
	if (uuids.size() != NUM_TESTS) {
		L_ERR("ERROR: Not all random UUIDs are different");
		++cont;
	}

	RETURN(cont);
}


int test_constructor_uuid() {
	int cont = 0;

	std::string u1("3c0f2be3-ff4f-40ab-b157-c51a81eff176");
	std::string u2("e47fcfdf-8db6-4469-a97f-57146dc41ced");
	std::string u3("b2ce58e8-d049-4705-b0cb-fe7435843781");

	UUID s1(u1);
	UUID s2(u2);
	UUID s3(u3);
	UUID s4(u1);

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


int test_special_uuids() {
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
		UUID uuid(uuid_orig);
		UUID uuid2 = UUID::unserialise(uuid.serialise());
		const auto uuid_rec = uuid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\t  Result: {}\n\tExpected: {}", uuid_rec, uuid_orig);
		}
	}

	RETURN(cont);
}


int test_compacted_uuids() {
	UUIDGenerator generator;
	int cont = 0;
	size_t min_length = 20, max_length = 0;
	for (int i = 0; i < NUM_TESTS; ++i) {
		UUID uuid = generator(true);
		const auto uuid_orig = uuid.to_string();
		const auto serialised = uuid.serialise();
		UUID uuid2 = UUID::unserialise(serialised);
		const auto uuid_rec = uuid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\t  Result: {}\n\tExpected: {}", uuid_rec, uuid_orig);
		}
		if (max_length < serialised.length()) {
			max_length = serialised.length();
		}
		if (min_length > serialised.length()) {
			min_length = serialised.length();
		}
	}

	if (max_length > MAX_COMPACTED_LENGTH) {
		L_ERR("ERROR: Max length for compacted uuid is {}", MAX_COMPACTED_LENGTH);
		++cont;
	}

	if (min_length < MIN_COMPACTED_LENGTH) {
		L_ERR("ERROR: Min length for compacted uuid is {}", MIN_COMPACTED_LENGTH);
		++cont;
	}

	RETURN(cont);
}


int test_condensed_uuids() {
	UUIDGenerator generator;
	int cont = 0;
	size_t min_length = 20, max_length = 0;
	for (int i = 0; i < NUM_TESTS; ++i) {
		UUID uuid = generator(false);
		const auto uuid_orig = uuid.to_string();
		const auto serialised = uuid.serialise();
		UUID uuid2 = UUID::unserialise(serialised);
		const auto uuid_rec = uuid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\t  Result: {}\n\tExpected: {}", uuid_rec, uuid_orig);
		}
		if (max_length < serialised.length()) {
			max_length = serialised.length();
		}
		if (min_length > serialised.length()) {
			min_length = serialised.length();
		}
	}

	if (max_length > MAX_CONDENSED_LENGTH) {
		L_ERR("ERROR: Max length for condensed uuid is {}", MAX_CONDENSED_LENGTH);
		++cont;
	}

	if (min_length < MIN_CONDENSED_LENGTH) {
		L_ERR("ERROR: Min length for condensed uuid is {}", MIN_CONDENSED_LENGTH);
		++cont;
	}

	RETURN(cont);
}


int test_expanded_uuids() {
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
		UUID uuid(uuid_orig);
		const auto serialised = uuid.serialise();
		UUID uuid2 = UUID::unserialise(serialised);
		const auto uuid_rec = uuid2.to_string();
		if (uuid_orig != uuid_rec) {
			++cont;
			L_ERR("ERROR:\n\t  Result: {}\n\tExpected: {}", uuid_rec, uuid_orig);
		}
		if (max_length < serialised.length()) {
			max_length = serialised.length();
		}
		if (min_length > serialised.length()) {
			min_length = serialised.length();
		}
	}

	if (max_length > MAX_EXPANDED_LENGTH) {
		L_ERR("ERROR: Max length for expanded uuid is {}", MAX_EXPANDED_LENGTH);
		++cont;
	}

	if (min_length < MIN_EXPANDED_LENGTH) {
		L_ERR("ERROR: Min length for expanded uuid is {}", MIN_EXPANDED_LENGTH);
		++cont;
	}

	RETURN(cont);
}


int test_several_uuids() {
	UUIDGenerator generator;
	size_t cont = 0;
	for (auto i = 0; i < NUM_TESTS; ++i) {
		std::vector<std::string> str_uuids;
		std::vector<std::string> norm_uuids;
		switch (i % 3) {
			case 0: {
				UUID uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back(uuid.to_string());

				uuid = generator(false);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back(uuid.to_string());

				uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back(uuid.to_string());

				uuid = generator(false);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back(uuid.to_string());

				uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back(uuid.to_string());
				break;
			}
			case 1: {
				UUID uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back("~" + B59.encode(uuid.serialise()));

				uuid = generator(false);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back("~" + B59.encode(uuid.serialise()));

				uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back("~" + B59.encode(uuid.serialise()));

				uuid = generator(false);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back("~" + B59.encode(uuid.serialise()));

				uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				norm_uuids.push_back("~" + B59.encode(uuid.serialise()));
				break;
			}
			default: {
				UUID uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				auto serialised = uuid.serialise();

				uuid = generator(false);
				str_uuids.push_back(uuid.to_string());
				serialised.append(uuid.serialise());

				uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				serialised.append(uuid.serialise());

				uuid = generator(false);
				str_uuids.push_back(uuid.to_string());
				serialised.append(uuid.serialise());

				uuid = generator(true);
				str_uuids.push_back(uuid.to_string());
				serialised.append(uuid.serialise());

				norm_uuids.push_back("~" + B59.encode(serialised));
				break;
			}
		}

		std::string uuids_serialised;
		for (auto& uuid : norm_uuids) {
			auto uuid_sz = uuid.size();
			if (uuid_sz) {
				if (uuid_sz == UUID_LENGTH) {
					try {
						uuids_serialised.append(UUID(uuid).serialise());
						continue;
					} catch (const std::invalid_argument&) { }
				}
				auto uuid_front = uuid.front();
				if (uuid_sz >= 7 && uuid_front == '~') {  // floor((4 * 8) / log2(59)) + 2
					try {
						auto decoded = B59.decode(uuid);
						if (UUID::is_serialised(decoded)) {
							uuids_serialised.append(decoded);
							continue;
						}
					} catch (const std::invalid_argument&) { }
				}
			}
			L_ERR("Invalid encoded UUID format: {}", repr(uuid));
		}

		std::string str_uuids_serialised;
		for (const auto& s : str_uuids) {
			str_uuids_serialised.append(UUID(s).serialise());
		}

		std::vector<UUID> uuids;
		UUID::unserialise(uuids_serialised, std::back_inserter(uuids));
		if (uuids.size() != str_uuids.size()) {
			++cont;
			L_ERR("ERROR: Different sizes: {} != {}\n\t  Result: {}\n\tExpected: {}", uuids.size(), str_uuids.size(), repr(uuids_serialised), repr(str_uuids_serialised));
		} else {
			auto it = str_uuids.begin();
			for (const auto& uuid : uuids) {
				const auto str_uuid = uuid.to_string();
				if (str_uuid != *it) {
					++cont;
					L_ERR("ERROR:\n\t  Result: {}\n\tExpected: {}", str_uuid, *it);
				}
				++it;
			}
		}
	}

	RETURN(cont);
}
