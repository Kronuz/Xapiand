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
#include "server_discovery.h"

#include <unistd.h>


EndpointList::EndpointList()
	: status(ST_NEW),
	  max_mastery_level(0),
	  init_timeout(0.005),
	  resolved_time(0),
	  stop_wait(false)
{
	pthread_cond_init(&time_cond, 0);
	pthread_mutexattr_init(&endl_qmtx_attr);
	pthread_mutexattr_settype(&endl_qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
	pthread_mutex_init(&endl_qmtx, &endl_qmtx_attr);
}


EndpointList::~EndpointList()
{
	pthread_mutex_destroy(&endl_qmtx);
	pthread_mutexattr_destroy(&endl_qmtx_attr);
	pthread_cond_destroy(&time_cond);
}


void EndpointList::add_endpoint(const Endpoint &element) {
	pthread_mutex_lock(&endl_qmtx);

	last_recv = now();

	endp_set.insert(element);

	double factor = 3.0;
	if (element.mastery_level > max_mastery_level) {
		max_mastery_level = element.mastery_level;
		factor = 2.0;
	}

	timespec_t elapsed = last_recv - init_time;
	next_wake = init_time + elapsed * factor;

	pthread_mutex_unlock(&endl_qmtx);

	pthread_cond_broadcast(&time_cond);
}


bool EndpointList::resolve_endpoint(const std::string &path, XapiandManager *manager, std::vector<Endpoint> &endpv, size_t n_endps, double timeout) {
	int initial_status, retval;
	timespec_t elapsed;

	pthread_mutex_lock(&endl_qmtx);

	initial_status = status;

	if (initial_status == ST_NEW) {
		elapsed = timeout;
	} else {
		elapsed = now() - init_time;

		if (elapsed > timeout && endp_set.empty()) {
			initial_status = ST_NEW;
			elapsed = timeout;
		} else if (elapsed > timeout * 10 && endp_set.size() < n_endps) {
			initial_status = ST_NEW;
			elapsed = timeout;
		} else if (elapsed > 3600.0) {
			manager->discovery(DISCOVERY_DB, serialise_string(path));
		}
	}

	while (elapsed <= timeout) {
		switch (initial_status) {
			case ST_NEW:
				clock_gettime(CLOCK_REALTIME, &init_time);

				manager->discovery(DISCOVERY_DB, serialise_string(path));

				next_wake = init_time + init_timeout;

				status = initial_status = ST_WAITING;

			case ST_WAITING:
				retval = pthread_cond_timedwait(&time_cond, &endl_qmtx, &next_wake);

				if (stop_wait) {
					elapsed = timeout + 1; // force it out
					stop_wait = false;
					break;
				}

				if (retval == ETIMEDOUT) {
					elapsed = now() - init_time;

					if (elapsed < timeout) {
						if (endp_set.size() < n_endps) {
							elapsed *= 3.0;
							if (elapsed >= timeout) {
								elapsed = timeout;
							}
							next_wake = init_time + elapsed;
						} else {
							elapsed = timeout + 1; // force it out
						}
					}
				} else if (retval) {
					LOG_ERR(this, "ERROR: pthread_cond_timedwait: %s\n", strerror(retval));
				}
				break;
		}
	}

	if (endp_set.empty()) {
		pthread_mutex_unlock(&endl_qmtx);
		return false;
	}

	if (resolved_time == 0) {
		resolved_time = time(NULL);
	}

	bool ret = get_endpoints(manager, n_endps, &endpv, NULL);

	pthread_mutex_unlock(&endl_qmtx);

	return ret;
}


bool EndpointList::get_endpoints(XapiandManager *manager, size_t n_endps, std::vector<Endpoint> *endpv, const Node **last_node)
{
	if (resolved_time != 0) {
		return false;
	}

	bool find_endpoints = false;
	if (endpv) endpv->clear();
	std::set<Endpoint, Endpoint::compare>::const_iterator it_endp(endp_set.cbegin());
	for (size_t c = 1; c <= n_endps && it_endp != endp_set.cend(); it_endp++, c++) {
		try {
			const Node *node;
			if (!manager->get_node((*it_endp).node_name, &node)) {
				return false;
			}
			if (node->touched > time(NULL) + HEARTBEAT_MAX) {
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


size_t EndpointList::size() {
	return endp_set.size();
}


bool EndpointList::empty() {
	return endp_set.empty();
}

void EndpointList::wakeup() {
	if (resolved_time != 0) {
		stop_wait = true;
		pthread_cond_broadcast(&time_cond);
	}
}

void EndpointList::show_list()
{
	pthread_mutex_lock(&endl_qmtx);
	std::set<Endpoint, Endpoint::compare>::iterator it(endp_set.begin());
	for ( ; it != endp_set.end(); it++) {
		LOG(this, "Endpoint list: --%s--\n", (*it).host.c_str());
	}
	pthread_mutex_unlock(&endl_qmtx);
}


void EndpointResolver::add_index_endpoint(const Endpoint &index, bool renew, bool wakeup)
{
	pthread_mutex_lock(&re_qmtx);
	get_action = renew ? EndpointResolver::renew : EndpointResolver::leave;
	EndpointList &enl = (*this)[index.path];
	get_action = EndpointResolver::renew;
	pthread_mutex_unlock(&re_qmtx);

	enl.add_endpoint(index);

	if (wakeup) enl.wakeup();
}


bool EndpointResolver::resolve_index_endpoint(const std::string &path, XapiandManager *manager, std::vector<Endpoint> &endpv, size_t n_endps, double timeout)
{
	pthread_mutex_lock(&re_qmtx);
	EndpointList &enl = (*this)[path];
	pthread_mutex_unlock(&re_qmtx);

	return enl.resolve_endpoint(path, manager, endpv, n_endps, timeout);
}


bool EndpointResolver::get_master_node(const std::string &index, const Node **node, XapiandManager *manager)
{
	pthread_mutex_lock(&re_qmtx);
	EndpointList &enl = (*this)[index];
	pthread_mutex_unlock(&re_qmtx);

	return enl.get_endpoints(manager, 1, NULL, node);
}
