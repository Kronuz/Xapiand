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

#ifndef __Xapiand__resolve_endpoint__
#define __Xapiand__resolve_endpoint__

#include "endpoint.h"
#include "lru.h"
#include "length.h"
#include "times.h"

#include <queue>
#include <pthread.h>

#define ST_NEW 0
#define ST_READY 1
#define ST_READY_TIME_OUT 2
#define ST_WAITING 3
#define ST_NEW_ENDP 4

class XapiandManager;

class EndpointList {
	std::set<Endpoint, Endpoint::compare> endp_set;

	pthread_cond_t time_cond;
	pthread_mutex_t endl_qmtx;
	pthread_mutexattr_t endl_qmtx_attr;

	timespec_t init_time;
	timespec_t last_recv;
	timespec_t next_wake;

	int status;
	int max_mastery_level;
	double init_timeout;

public:
	EndpointList();
	~EndpointList();
	bool resolve_endpoint(const std::string &path, XapiandManager *manager, std::vector<Endpoint> &endpv, int n_endps, double timeout);
	void add_endpoint(const Endpoint &element);
	bool _get_endpoints(std::vector<Endpoint> &Endv, int n_endps);
	size_t size();
	bool empty();
	void show_list();
};



class EndpointResolver : public lru_map<std::string, EndpointList>{

	pthread_mutex_t re_qmtx;
	pthread_mutexattr_t re_qmtx_attr;


public:
	void add_index_endpoint(Endpoint index);
	bool resolve_index_endpoint(const std::string &path, XapiandManager *manager, std::vector<Endpoint> &endpv, int n_endps=1, double timeout=1.0);
	EndpointResolver()
	{
		pthread_mutexattr_init(&re_qmtx_attr);
		pthread_mutexattr_settype(&re_qmtx_attr, PTHREAD_MUTEX_RECURSIVE);
		pthread_mutex_init(&re_qmtx, &re_qmtx_attr);
	}

	~EndpointResolver()
	{
		pthread_mutex_destroy(&re_qmtx);
		pthread_mutexattr_destroy(&re_qmtx_attr);
	}
};

#endif /* defined(__Xapiand__resolve_endpoint__) */
