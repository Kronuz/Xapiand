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

#include <unistd.h>
#include <sys/time.h>


EndpointList::EndpointList()
	: status(ST_NEW),
	  max_mastery_level(0),
	  timeout(1),
	  init_timeout(0.005)
{
	memset(&init_time, 0, sizeof (init_time));
	memset(&last_recv, 0, sizeof (last_recv));
	memset(&current_time, 0, sizeof (current_time));
	memset(&next_wake, 0, sizeof (next_wake));
	
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
	int factor = 3;

	struct timeval result;
	memset(&result, 0, sizeof result);

	pthread_mutex_lock(&endl_qmtx);

	endp_set.insert(element);

	gettimeofday(&last_recv, NULL);

	double elapsed = last_recv.tv_sec + (double)last_recv.tv_usec / 1e6;
	elapsed -= init_time.tv_sec + (double)init_time.tv_usec / 1e6;

	if (elapsed >= timeout) {
		LOG(this, "Paso 5a\n");
		status = ST_READY_TIME_OUT;
	} else {
		LOG(this, "Paso 5b\n");
		if (element.mastery_level > max_mastery_level) {
			max_mastery_level = element.mastery_level;
			factor = 2;
		}

		elapsed *= factor;
		if (elapsed >= timeout) {
			elapsed = timeout;
		}

		next_wake.tv_nsec = (init_time.tv_usec * 1e3) + int((elapsed - int(elapsed)) * 1e9);
		next_wake.tv_sec = init_time.tv_sec + (int)elapsed +  + (next_wake.tv_nsec / 1e9);
		next_wake.tv_nsec %= (long)1e9;
	}

	pthread_mutex_unlock(&endl_qmtx);

	pthread_cond_broadcast(&time_cond);
}


bool EndpointList::resolve_endpoint(const std::string &path, HttpClient *client, std::vector<Endpoint> &endpv, int n_endps){
	int retval;

	pthread_mutex_lock(&endl_qmtx);

	while (status != ST_READY && status != ST_READY_TIME_OUT) {
		switch (status) {
			case ST_NEW:
				retval = gettimeofday(&init_time, NULL);

				client->manager()->discovery(DISCOVERY_DB, serialise_string(path));

				next_wake.tv_nsec = (init_time.tv_usec * 1e3) + int((init_timeout - int(init_timeout)) * 1e9);
				next_wake.tv_sec = init_time.tv_sec + (int)init_timeout + (next_wake.tv_nsec / (long)1e9);
				next_wake.tv_nsec %= (long)1e9;

				status = ST_WAITING;

			case ST_WAITING:
				retval = pthread_cond_timedwait(&time_cond, &endl_qmtx, &next_wake);
				if (retval == ETIMEDOUT) {
					gettimeofday(&current_time, NULL);
					double elapsed = current_time.tv_sec + (double)current_time.tv_usec / 1e6;
					elapsed -= init_time.tv_sec + (double)init_time.tv_usec / 1e6;
					
					if (elapsed >= timeout) {
						LOG(this,"Time wait over\n");
						status = ST_READY_TIME_OUT;
					} else {
						if (endp_set.size() < n_endps) {
							elapsed *= 3.0;
							if (elapsed >= timeout) {
								elapsed = timeout;
							}
							next_wake.tv_nsec = (init_time.tv_usec * 1e3) + int((elapsed - int(elapsed)) * 1e9);
							next_wake.tv_sec = init_time.tv_sec + (int)elapsed + (next_wake.tv_nsec / 1e9);
							next_wake.tv_nsec %= (long)1e9;
						} else {
							status = ST_READY;
						}
					}
				} else if (retval) {
					LOG_ERR(this, "ERROR: pthread_cond_timedwait: %s\n", strerror(retval));
				}
				break;
		}
	}

	if (status == ST_READY_TIME_OUT) {
		pthread_mutex_unlock(&endl_qmtx);
		return false;
	}

	bool ret = _get_endpoints(endpv, n_endps);

	pthread_mutex_unlock(&endl_qmtx);

	return ret;
}


bool EndpointList::_get_endpoints(std::vector<Endpoint> &Endv, int n_endps) {
	int c;
	std::set<Endpoint, Endpoint::compare>::const_iterator it_endp = endp_set.cbegin();
	for (c = 1; c <= n_endps && it_endp != endp_set.cend(); it_endp++) {
		Endv.push_back(*it_endp);
	}
	return c == n_endps;
}


size_t EndpointList::size() {
	return endp_set.size();
}


bool EndpointList::empty() {
	return endp_set.empty();
}


void EndpointList::show_list()
{
	pthread_mutex_lock(&endl_qmtx);
	std::set<Endpoint, Endpoint::compare>::iterator it;
	for (it = endp_set.begin(); it != endp_set.end(); it++) {
		LOG(this,"Endpoint list: --%s--\n",(*it).host.c_str());
	}
	pthread_mutex_unlock(&endl_qmtx);
}

void EndpointResolver::add_index_endpoint(Endpoint index)
{
	pthread_mutex_lock(&re_qmtx);
	EndpointList &enl = (*this)[index.path];
	pthread_mutex_unlock(&re_qmtx);

	enl.add_endpoint(index);
}


bool EndpointResolver::resolve_index_endpoint(const std::string &path, HttpClient *client, std::vector<Endpoint> &endpv, int n_endps)
{
	pthread_mutex_lock(&re_qmtx);
	EndpointList &enl = (*this)[path];
	pthread_mutex_unlock(&re_qmtx);

	return enl.resolve_endpoint(path, client, endpv, n_endps);
}
