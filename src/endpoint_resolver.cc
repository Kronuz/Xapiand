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

#include "endpoint_resolver.h"

#include "client_http.h"
#include "servers/discovery.h"

#include <unistd.h>


EndpointList::EndpointList()
	: status(State::NEW),
	  max_mastery_level(0),
	  init_timeout(5),
	  stop_wait(false) { }


EndpointList::EndpointList(EndpointList&& other) {
	std::lock_guard<std::mutex> lk(other.endl_qmtx);
	status = std::move(other.status);
	max_mastery_level = std::move(other.max_mastery_level);
	init_timeout = std::move(other.init_timeout);
	stop_wait = std::move(other.stop_wait);
}


void
EndpointList::add_endpoint(const Endpoint &element)
{
	std::unique_lock<std::mutex> lk(endl_qmtx);

	last_recv = system_clock::now();

	endp_set.insert(element);

	double factor = 3.0;
	if (element.mastery_level > max_mastery_level) {
		max_mastery_level = element.mastery_level;
		factor = 2.0;
	}

	auto elapsed = duration<double, std::milli>(last_recv - init_time);
	next_wake = init_time + elapsed * factor;

	lk.unlock();

	time_cond.notify_all();
}


bool EndpointList::resolve_endpoint(const std::string &path, std::shared_ptr<XapiandManager> manager, std::vector<Endpoint> &endpv, size_t n_endps, duration<double, std::milli> timeout)
{
	State initial_status;
	std::cv_status retval;
	duration<double, std::milli> elapsed;

	std::unique_lock<std::mutex> lk(endl_qmtx);

	initial_status = status;

	if (initial_status == State::NEW) {
		elapsed = timeout;
	} else {
		elapsed = system_clock::now() - init_time;

		if (elapsed > timeout && endp_set.empty()) {
			initial_status = State::NEW;
			elapsed = timeout;
		} else if (elapsed > timeout * 10 && endp_set.size() < n_endps) {
			initial_status = State::NEW;
			elapsed = timeout;
		} else if (elapsed > 1h) {
			manager->discovery->send_message(Discovery::Message::DB, serialise_string(path));
		}
	}

	while (elapsed <= timeout) {
		switch (initial_status) {
			case State::NEW:
				init_time = system_clock::now();

				manager->discovery->send_message(Discovery::Message::DB, serialise_string(path));

				next_wake = init_time + init_timeout;

				status = initial_status = State::WAITING;

			case State::WAITING:
				retval = time_cond.wait_until(lk, next_wake);

				if (stop_wait) {
					elapsed = timeout + 1s; // force it out
					stop_wait = false;
					break;
				}

				if (retval == std::cv_status::timeout) {
					elapsed = system_clock::now() - init_time;

					if (elapsed < timeout) {
						if (endp_set.size() < n_endps) {
							elapsed *= 3.0;
							if (elapsed >= timeout) {
								elapsed = timeout;
							}
							next_wake = init_time + elapsed;
						} else {
							elapsed = timeout + 1s; // force it out
						}
					}
				}
				break;
			default: break;
		}
	}

	if (endp_set.empty()) {
		return false;
	}

	bool ret = get_endpoints(std::move(manager), n_endps, &endpv, nullptr);

	return ret;
}


bool EndpointList::get_endpoints(std::shared_ptr<XapiandManager> manager, size_t n_endps, std::vector<Endpoint> *endpv, const Node **last_node)
{
	bool find_endpoints = false;
	if (endpv) endpv->clear();
	std::set<Endpoint, Endpoint::compare>::const_iterator it_endp(endp_set.cbegin());
	for (size_t c = 1; c <= n_endps && it_endp != endp_set.cend(); it_endp++, c++) {
		try {
			const Node *node = nullptr;
			if (!manager->get_node((*it_endp).node_name, &node)) {
				return false;
			}
			if (node->touched > epoch::now<>() + HEARTBEAT_MAX) {
				return false;
			}
			if (last_node) *last_node = node;
		} catch (const std::out_of_range &err) {
			continue;
		}
		if (c == n_endps) {
			find_endpoints = true;
		}
		if (endpv) endpv->push_back(*it_endp);
	}
	return find_endpoints;
}


size_t
EndpointList::size()
{
	return endp_set.size();
}


bool
EndpointList::empty()
{
	return endp_set.empty();
}


void
EndpointList::wakeup()
{
	stop_wait = true;
	time_cond.notify_all();
}


void
EndpointList::show_list()
{
	std::lock_guard<std::mutex> lk(endl_qmtx);
	std::set<Endpoint, Endpoint::compare>::iterator it(endp_set.begin());
	for ( ; it != endp_set.end(); it++) {
		L_DEBUG(this, "Endpoint list: --%s--", (*it).host.c_str());
	}
}


void
EndpointResolver::add_index_endpoint(const Endpoint &index, bool renew, bool wakeup)
{
	std::unique_lock<std::mutex> lk(re_qmtx);
	action = renew ? lru::GetAction::renew : lru::GetAction::leave;
	EndpointList &enl = (*this)[index.path];
	action = lru::GetAction::renew;
	lk.unlock();

	enl.add_endpoint(index);

	if (wakeup) enl.wakeup();
}


bool EndpointResolver::resolve_index_endpoint(const std::string &path, std::shared_ptr<XapiandManager> manager, std::vector<Endpoint> &endpv, size_t n_endps, duration<double, std::milli> timeout)
{
	std::unique_lock<std::mutex> lk(re_qmtx);
	EndpointList &enl = (*this)[path];
	lk.unlock();

	return enl.resolve_endpoint(path, std::move(manager), endpv, n_endps, timeout);
}


bool EndpointResolver::get_master_node(const std::string &index, const Node **node, std::shared_ptr<XapiandManager> manager)
{
	std::unique_lock<std::mutex> lk(re_qmtx);
	EndpointList &enl = (*this)[index];
	lk.unlock();

	return enl.get_endpoints(std::move(manager), 1, nullptr, node);
}
