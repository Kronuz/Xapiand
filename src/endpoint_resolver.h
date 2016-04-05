/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "xapiand.h"

#ifdef XAPIAND_CLUSTERING

#include "endpoint.h"
#include "lru.h"
#include "length.h"
#include "times.h"

#include <queue>
#include <mutex>
#include <condition_variable>

class XapiandManager;

using namespace std::chrono;
using namespace std::literals;


class EndpointList {
	friend class EndpointResolver;

	enum class State {
		NEW,
		READY,
		READY_TIME_OUT,
		WAITING,
		NEW_ENDP
	};

	std::set<Endpoint, Endpoint::compare> endp_set;

	std::condition_variable time_cond;
	std::mutex endl_qmtx;

	system_clock::time_point init_time;
	system_clock::time_point last_recv;
	time_point<system_clock, duration<double, std::milli>> next_wake;

	State status;
	long long max_mastery_level;

	duration<double, std::milli> init_timeout;
	bool stop_wait;

	bool get_endpoints(std::shared_ptr<XapiandManager> manager, size_t n_endps, std::vector<Endpoint> *endpv, std::shared_ptr<const Node>* last_node);

	bool resolve_endpoint(const std::string &path, std::shared_ptr<XapiandManager> manager, std::vector<Endpoint> &endpv, size_t n_endps, duration<double, std::milli> timeout);

	void add_endpoint(const Endpoint &element);
	void wakeup();

	size_t size();
	bool empty();
	void show_list();

public:
	EndpointList();
	EndpointList(EndpointList&& other);
	~EndpointList() = default;
};



class EndpointResolver : public lru::LRU<std::string, EndpointList> {
	std::mutex re_qmtx;

	lru::GetAction action;

	EndpointList& operator[] (const std::string& key) {
		try {
			return at_and([this](EndpointList&) {
				return action;
			}, key);
		} catch (const std::range_error&) {
			return insert(std::make_pair(key, EndpointList()));
		}
	 }

public:
	void add_index_endpoint(const Endpoint &index, bool renew, bool wakeup);

	bool resolve_index_endpoint(const std::string &path, std::shared_ptr<XapiandManager> manager, std::vector<Endpoint> &endpv, size_t n_endps=1, duration<double, std::milli> timeout=1s);

	bool get_master_node(const std::string &index, std::shared_ptr<const Node>* node, std::shared_ptr<XapiandManager> manager);

	EndpointResolver(size_t max_size) :
	    LRU<std::string, EndpointList>(max_size),
	    action(lru::GetAction::renew) {}

	~EndpointResolver() = default;
};

#endif
