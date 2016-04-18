/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "test_serialise_stl.h"

#include "../src/log.h"
#include "../src/stl_serialise.h"
#include "utils.h"


int unserialise_to_StringList(const std::string& _serialise, size_t expected_size) {
	StringList u_sl;
	u_sl.unserialise(_serialise);
	return u_sl.size() == expected_size ? 0 : 1;
}


template <typename T, typename = std::enable_if_t<std::is_same<StringList, std::decay_t<T>>::value ||
		std::is_same<StringSet, std::decay_t<T>>::value>>
int unserialise_to_StringList(T&& sl) {
	StringList u_sl;
	u_sl.unserialise(sl.serialise());
	if (sl.size() == u_sl.size()) {
		auto it = u_sl.begin();
		int cont = 0;
		for (const auto& val : sl) {
			if (val != *it) {
				L_ERR(nullptr, "ERROR: In StringList, differents values. Expected: %s  Result: %s\n", val.c_str(), it->c_str());
				++cont;
			}
			++it;
		}
		return cont;
	}

	L_ERR(nullptr, "ERROR: In StringList, differents sizes. Expected: %zu  Result: %zu\n", sl.size(), u_sl.size());
	return 1;
}


int unserialise_to_StringSet(const std::string& _serialise, size_t expected_size) {
	StringSet u_ss;
	u_ss.unserialise(_serialise);
	return u_ss.size() == expected_size ? 0 : 1;
}


template <typename T, typename = std::enable_if_t<std::is_same<StringList, std::decay_t<T>>::value ||
		std::is_same<StringSet, std::decay_t<T>>::value>>
int unserialise_to_StringSet(T&& ss) {
	StringSet expected;
	expected.insert(ss.begin(), ss.end());

	StringSet u_ss;
	u_ss.unserialise(ss.serialise());
	if (expected.size() == u_ss.size()) {
		auto it = u_ss.begin();
		int cont = 0;
		for (const auto& val : expected) {
			if (val != *it) {
				L_ERR(nullptr, "ERROR: In StringList, differents values. Expected: %s  Result: %s\n", val.c_str(), it->c_str());
				++cont;
			}
			++it;
		}
		return cont;
	}

	L_ERR(nullptr, "ERROR: In StringList, differents sizes. Expected: %zu  Result: %zu\n", expected.size(), u_ss.size());
	return 1;
}


int unserialise_to_CartesianUSet(const std::string& _serialise, size_t expected_size) {
	CartesianUSet c_uset;
	c_uset.unserialise(_serialise);
	return c_uset.size() == expected_size ? 0 : 1;
}


template <typename T, typename = std::enable_if_t<std::is_same<CartesianUSet, std::decay_t<T>>::value>>
int unserialise_to_CartesianUSet(T&& c_uset) {
	CartesianUSet uc_uset;
	uc_uset.unserialise(c_uset.serialise());
	if (c_uset.size() == uc_uset.size()) {
		int cont = 0;
		for (const auto& val : c_uset) {
			if (uc_uset.find(val) == uc_uset.end()) {
				L_ERR(nullptr, "ERROR: In CartesianUSet, differents values.\n");
				++cont;
			}
		}
		return cont;
	}

	L_ERR(nullptr, "ERROR: In CartesianUSet, differents sizes. Expected: %zu  Result: %zu\n", c_uset.size(), uc_uset.size());
	return 1;
}


int unserialise_to_RangeList(const std::string& _serialise, size_t expected_size) {
	RangeList rl2;
	rl2.unserialise(_serialise);
	return rl2.size() == expected_size ? 0 : 1;
}


template <typename T, typename = std::enable_if_t<std::is_same<RangeList, std::decay_t<T>>::value>>
int unserialise_to_RangeList(T&& rl) {
	RangeList url;
	url.unserialise(rl.serialise());
	if (rl.size() == url.size()) {
		auto it = url.begin();
		int cont = 0;
		for (const auto& val : rl) {
			if (val.start != it->start || val.end != it->end) {
				L_ERR(nullptr, "ERROR: In RangeList, differents values. Expected: { %lld, %lld }  Result: { %lld, %lld }\n", val.start, val.end, it->start, it->end);
				++cont;
			}
			++it;
		}
		return cont;
	}

	L_ERR(nullptr, "ERROR: In RangeList, differents sizes. Expected: %zu  Result: %zu\n", rl.size(), url.size());
	return 1;
}


int test_StringList() {
	StringList sl;

	// Empty StringList
	int cont = unserialise_to_StringList(sl);
	cont += unserialise_to_StringSet(sl);
	std::string _serialise = sl.serialise();
	cont += unserialise_to_StringList(_serialise, 0);
	cont += unserialise_to_StringSet(_serialise, 0);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	// StringList with data
	sl.emplace_back("c");
	sl.emplace_back("b");
	sl.emplace_back("g");
	sl.emplace_back("e");
	sl.emplace_back("j");
	sl.emplace_back("m");
	sl.emplace_back("k");
	sl.emplace_back("l");
	sl.emplace_back("a");

	auto _size = sl.size();

	cont = unserialise_to_StringList(sl);
	cont += unserialise_to_StringSet(sl);
	_serialise = sl.serialise();
	cont += unserialise_to_StringList(_serialise, _size);
	cont += unserialise_to_StringSet(_serialise, _size);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	StringList sl2;
	sl2.push_back("z");
	sl2.push_back("y");
	sl2.push_back("x");
	sl2.push_back("w");

	_size += sl2.size();
	sl.add_unserialise(sl2.serialise());
	if (sl.size() != _size) {
		L_ERR(nullptr, "ERROR: In StringList::add_unserialise, differents sizes. Expected: %zu  Result: %zu\n", _size, sl.size());
		++cont;
	}

		RETURN(cont);

}


int test_StringSet() {
	StringSet ss;

	// Empty StringSet
	int cont = unserialise_to_StringList(ss);
	cont += unserialise_to_StringSet(ss);
	std::string _serialise = ss.serialise();
	cont += unserialise_to_StringList(_serialise, 0);
	cont += unserialise_to_StringSet(_serialise, 0);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	// StringSet with data
	ss.insert("c");
	ss.insert("b");
	ss.insert("c");
	ss.insert("e");
	ss.insert("j");
	ss.insert("b");
	ss.insert("k");
	ss.insert("l");
	ss.insert("a");

	auto _size = ss.size();

	cont += unserialise_to_StringList(ss);
	cont += unserialise_to_StringSet(ss);
	_serialise = ss.serialise();
	cont += unserialise_to_StringList(_serialise, _size);
	cont += unserialise_to_StringSet(_serialise, _size);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	StringSet ss2;
	ss2.insert("z");
	ss2.insert("y");
	ss2.insert("x");
	ss2.insert("w");

	_size += ss2.size();
	ss.add_unserialise(ss2.serialise());
	if (ss.size() != _size) {
		L_ERR(nullptr, "ERROR: In StringSet::add_unserialise, differents sizes. Expected: %zu  Result: %zu\n", _size, ss.size());
		++cont;
	}

	RETURN(cont);
}


int test_CartesianUSet() {
	CartesianUSet c_uset;

	// Empty CartesianUSet.
	int cont = unserialise_to_CartesianUSet(c_uset);
	std::string _serialise = c_uset.serialise();
	cont += unserialise_to_StringList(_serialise, 0);
	cont += unserialise_to_StringSet(_serialise, 0);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	// CartesianUSet witd data.
	c_uset.insert(Cartesian( 0.925602814,  0.336891873,  0.172520422));
	c_uset.insert(Cartesian( 0.837915107,  0.224518676,  0.497483301));
	c_uset.insert(Cartesian( 0.665250371,  0.384082481,  0.640251974));
	c_uset.insert(Cartesian( 0.765933665,  0.407254153,  0.497483341));
	c_uset.insert(Cartesian( 0.925602814, -0.336891873, -0.172520422));
	c_uset.insert(Cartesian( 0.837915107,  0.224518676, -0.497483301));
	c_uset.insert(Cartesian( 0.665250371, -0.384082481,  0.640251974));
	c_uset.insert(Cartesian( 0.765933705,  0.407254175,  0.497483262));
	c_uset.insert(Cartesian(-0.765933705, -0.407254175, -0.497483262));

	auto _size = c_uset.size();

	cont += unserialise_to_CartesianUSet(c_uset);
	_serialise = c_uset.serialise();
	cont += unserialise_to_StringList(_serialise, 1);
	cont += unserialise_to_StringSet(_serialise, 1);
	cont += unserialise_to_CartesianUSet(_serialise, _size);
	cont += unserialise_to_RangeList(_serialise, 0);

	c_uset.add_unserialise(_serialise);
	if (c_uset.size() != _size) {
		L_ERR(nullptr, "ERROR: In CartesianUSet::add_unserialise, differents sizes. Expected: %zu  Result: %zu\n", _size, c_uset.size());
		++cont;
	}

	RETURN(cont);
}


int test_RangeList() {
	RangeList rl;

	// Empty RangeList.
	int cont = unserialise_to_RangeList(rl);
	std::string _serialise = rl.serialise();
	cont += unserialise_to_StringList(_serialise, 0);
	cont += unserialise_to_StringSet(_serialise, 0);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	// RangeList with data.
	rl.push_back({ 100, 200 });
	rl.push_back({ 300, 400 });
	rl.push_back({ 600, 900 });
	rl.push_back({ 100, 400 });
	rl.push_back({ 800, 900 });

	auto _size = rl.size();

	cont += unserialise_to_RangeList(rl);
	_serialise = rl.serialise();
	cont += unserialise_to_StringList(_serialise, 1);
	cont += unserialise_to_StringSet(_serialise, 1);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, _size);

	_size *= 2;
	rl.add_unserialise(_serialise);
	if (rl.size() != _size) {
		L_ERR(nullptr, "ERROR: In RangeList::add_unserialise, differents sizes. Expected: %zu  Result: %zu\n", _size, rl.size());
		++cont;
	}

	RETURN(cont);
}
