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

#include "test_dllist.h"

#include "utils.h"

#include <thread>


#include <random>
#include <string>
#include <thread>
#include <vector>


std::string repr_results(const DLList<std::pair<int, char>>& l, bool sort) {
	std::vector<std::string> res;
	for (const auto& elem : l) {
		res.push_back(std::string(1, elem.second) + std::to_string(elem.first));
	}

	if (sort) {
		std::sort(res.begin(), res.end());
	}

	std::string out;
	for (auto it = res.begin(); it != res.end(); ) {
		out += *it++;
		if (it != res.end()) {
			out += " ";
		}
	}

	return out;
}


int test_push_front() {
	DLList<std::pair<int, char>> mylist;

	mylist.push_front(std::make_pair<int, char>(1, 'a'));
	mylist.push_front(std::make_pair<int, char>(1, 'b'));
	mylist.push_front(std::make_pair<int, char>(1, 'c'));
	mylist.push_front(std::make_pair<int, char>(1, 'd'));
	mylist.push_front(std::make_pair<int, char>(1, 'e'));
	mylist.push_front(std::make_pair<int, char>(1, 'f'));
	mylist.push_front(std::make_pair<int, char>(1, 'g'));
	mylist.push_front(std::make_pair<int, char>(1, 'h'));
	mylist.push_front(std::make_pair<int, char>(1, 'i'));
	mylist.push_front(std::make_pair<int, char>(1, 'j'));
	mylist.push_front(std::make_pair<int, char>(1, 'k'));

	std::string result = repr_results(mylist, false);

	std::string expected("k1 j1 i1 h1 g1 f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_emplace_front() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');

	std::string result = repr_results(mylist, false);

	std::string expected("k1 j1 i1 h1 g1 f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_push_back() {
	DLList<std::pair<int, char>> mylist;

	mylist.push_back(std::make_pair<int, char>(1, 'a'));
	mylist.push_back(std::make_pair<int, char>(1, 'b'));
	mylist.push_back(std::make_pair<int, char>(1, 'c'));
	mylist.push_back(std::make_pair<int, char>(1, 'd'));
	mylist.push_back(std::make_pair<int, char>(1, 'e'));
	mylist.push_back(std::make_pair<int, char>(1, 'f'));
	mylist.push_back(std::make_pair<int, char>(1, 'g'));
	mylist.push_back(std::make_pair<int, char>(1, 'h'));
	mylist.push_back(std::make_pair<int, char>(1, 'i'));
	mylist.push_back(std::make_pair<int, char>(1, 'j'));
	mylist.push_back(std::make_pair<int, char>(1, 'k'));

	std::string result = repr_results(mylist, false);

	std::string expected("a1 b1 c1 d1 e1 f1 g1 h1 i1 j1 k1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_emplace_back() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_back(1, 'a');
	mylist.emplace_back(1, 'b');
	mylist.emplace_back(1, 'c');
	mylist.emplace_back(1, 'd');
	mylist.emplace_back(1, 'e');
	mylist.emplace_back(1, 'f');
	mylist.emplace_back(1, 'g');
	mylist.emplace_back(1, 'h');
	mylist.emplace_back(1, 'i');
	mylist.emplace_back(1, 'j');
	mylist.emplace_back(1, 'k');

	std::string result = repr_results(mylist, false);

	std::string expected("a1 b1 c1 d1 e1 f1 g1 h1 i1 j1 k1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_pop_front() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');
	mylist.emplace_front(1, 'l');

	int del = mylist.size() / 2;
	for (int i = 0; i < del; ++i) {
		mylist.pop_front();
	}

	std::string result = repr_results(mylist, false);

	std::string expected("f1 e1 d1 c1 b1 a1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_pop_back() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');
	mylist.emplace_front(1, 'l');

	int del = mylist.size() / 2;
	for (int i = 0; i < del; ++i) {
		mylist.pop_back();
	}

	std::string result = repr_results(mylist, false);

	std::string expected("l1 k1 j1 i1 h1 g1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_erase() {
	DLList<std::pair<int, char>> mylist;

	mylist.emplace_front(1, 'a');
	mylist.emplace_front(1, 'b');
	mylist.emplace_front(1, 'c');
	mylist.emplace_front(1, 'd');
	mylist.emplace_front(1, 'e');
	mylist.emplace_front(1, 'f');
	mylist.emplace_front(1, 'g');
	mylist.emplace_front(1, 'h');
	mylist.emplace_front(1, 'i');
	mylist.emplace_front(1, 'j');
	mylist.emplace_front(1, 'k');
	mylist.emplace_front(1, 'l');

	int cont = 0;
	for (auto it = mylist.begin(); it != mylist.end();) {
		if (cont % 2 == 0) {
			it = mylist.erase(it);
		} else {
			++it;
		}
		++cont;
	}

	std::string result = repr_results(mylist, false);

	std::string expected("k1 i1 g1 e1 c1 a1");

	if (result == expected) {
		return 0;
	} else {
		fprintf(stderr, "Result: { %s }  Expected: { %s }", result.c_str(), expected.c_str());
		return 1;
	}
}


int test_single_producer_consumer() {
	DLList<std::pair<int, char>> mylist;
	int err = 0;

	// Test several inserts
	size_t elements = 0;
	for (char c = 'a'; c <= 'z'; ++c) {
		elements += 40 * c;
		for (int i = 0; i < 10 * c; ++i) {
			std::pair<int, char> ele(i, c);
			mylist.push_front(ele);
			mylist.push_back(ele);
			mylist.emplace_front(ele);
			mylist.emplace_back(ele);
		}
	}

	// Test size
	if (elements != mylist.size()) {
		fprintf(stderr, "ForwardList::size is not working!");
		++err;
	}

	// Test several erases.
	for (int i = 0; i < 100; ++i) {
		mylist.pop_front();
		mylist.pop_back();
		elements -= 2;
	}

	// Test size
	if (elements != mylist.size()) {
		fprintf(stderr, "ForwardList::size is not working!");
		++err;
	}

	// Test clear
	mylist.clear();
	if (mylist.size() != 0) {
		fprintf(stderr, "ForwardList::clear is not working!");
		++err;
	}

	return err;
}


int test_multi_push_pop_front() {
	DLList<std::string> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < 20; ++i) {
		producers.emplace_back([&l](const std::string& val) {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 100);
			std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
			l.push_front(val);
			l.emplace_front(val);
		}, std::to_string(i));
	}

	std::atomic_size_t fail_pop(40);
	std::vector<std::thread> consumers;
	for (int i = 0; i < 20; ++i) {
		consumers.emplace_back([&l, &fail_pop]() {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 100);
			std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
			try {
				l.pop_front();
				--fail_pop;
			} catch (const std::out_of_range&) { }
		});
	}

	for (auto& producer : producers) {
		producer.join();
	}

	for (auto& consumer : consumers) {
		consumer.join();
	}

	fprintf(stderr, "Size List: %zu  Fail Pop: %zu\n", l.size(), fail_pop.load());

	return l.size() != fail_pop.load();
}


int test_multi_push_pop_back() {
	DLList<std::string> l;

	std::vector<std::thread> producers;
	for (int i = 0; i < 20; ++i) {
		producers.emplace_back([&l](const std::string& val) {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 100);
			std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
			l.push_back(val);
			l.emplace_back(val);
		}, std::to_string(i));
	}

	std::atomic_size_t fail_pop(40);
	std::vector<std::thread> consumers;
	for (int i = 0; i < 20; ++i) {
		consumers.emplace_back([&l, &fail_pop]() {
			std::random_device rd;
			std::mt19937 gen(rd());
			std::uniform_int_distribution<> dis(0, 100);
			std::this_thread::sleep_for(std::chrono::milliseconds(dis(gen)));
			try {
				l.pop_back();
				--fail_pop;
			} catch (const std::out_of_range&) { }
		});
	}

	for (auto& producer : producers) {
		producer.join();
	}

	for (auto& consumer : consumers) {
		consumer.join();
	}

	fprintf(stderr, "Size List: %zu  Fail Pop: %zu\n", l.size(), fail_pop.load());

	return l.size() != fail_pop.load();
}
