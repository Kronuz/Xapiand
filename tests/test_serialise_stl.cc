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
int unserialise_to_CartesianUSet(T&& c_uset, const std::set<std::string>& str_cartesian=std::set<std::string>()) {
	CartesianUSet uc_uset;
	uc_uset.unserialise(c_uset.serialise());
	if (c_uset.size() == uc_uset.size()) {
		auto it = uc_uset.begin();
		int cont = 0;
		for (const auto& val : uc_uset) {
			if (str_cartesian.find(val.as_string()) == str_cartesian.end()) {
				L_ERR(nullptr, "ERROR: In CartesianUSet, differents values.\n");
				++cont;
			}
			++it;
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
	sl.emplace_back("c");
	sl.emplace_back("e");
	sl.emplace_back("j");
	sl.emplace_back("b");
	sl.emplace_back("k");
	sl.emplace_back("l");
	sl.emplace_back("a");

	cont = unserialise_to_StringList(sl);
	cont += unserialise_to_StringSet(sl);
	_serialise = sl.serialise();
	cont += unserialise_to_StringList(_serialise, sl.size());
	cont += unserialise_to_StringSet(_serialise, 7);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	return cont;
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

	cont += unserialise_to_StringList(ss);
	cont += unserialise_to_StringSet(ss);
	_serialise = ss.serialise();
	cont += unserialise_to_StringList(_serialise, ss.size());
	cont += unserialise_to_StringSet(_serialise, ss.size());
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	return cont;
}


int test_CartesianUSet() {
	CartesianUSet c_uset;
	std::set<std::string> str_cartesian;

	// Empty CartesianUSet.
	int cont = unserialise_to_CartesianUSet(c_uset, str_cartesian);
	std::string _serialise = c_uset.serialise();
	cont += unserialise_to_StringList(_serialise, 0);
	cont += unserialise_to_StringSet(_serialise, 0);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 0);

	// CartesianUSet witd data.
	Cartesian c(10, 20, 0, CartesianUnits::DEGREES);
	c.normalize();
	str_cartesian.insert(c.as_string());
	c_uset.insert(c);
	c = Cartesian(30, 50, 0, CartesianUnits::DEGREES);
	c.normalize();
	str_cartesian.insert(c.as_string());
	c_uset.insert(c);
	c = Cartesian(15, 25, 0, CartesianUnits::DEGREES);
	c.normalize();
	str_cartesian.insert(c.as_string());
	c_uset.insert(c);
	c = Cartesian(-10, -20, 0, CartesianUnits::DEGREES);
	c.normalize();
	str_cartesian.insert(c.as_string());
	c_uset.insert(c);
	c = Cartesian(0, 0, 0, CartesianUnits::DEGREES);
	c.normalize();
	str_cartesian.insert(c.as_string());
	c_uset.insert(c);

	cont += unserialise_to_CartesianUSet(c_uset, str_cartesian);
	_serialise = c_uset.serialise();
	cont += unserialise_to_StringList(_serialise, 1);
	cont += unserialise_to_StringSet(_serialise, 1);
	cont += unserialise_to_CartesianUSet(_serialise, 5);
	cont += unserialise_to_RangeList(_serialise, 0);

	return cont;
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

	cont += unserialise_to_RangeList(rl);
	_serialise = rl.serialise();
	cont += unserialise_to_StringList(_serialise, 1);
	cont += unserialise_to_StringSet(_serialise, 1);
	cont += unserialise_to_CartesianUSet(_serialise, 0);
	cont += unserialise_to_RangeList(_serialise, 5);

	return cont;
}
