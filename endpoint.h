/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#ifndef XAPIAND_INCLUDED_ENDPOINT_H
#define XAPIAND_INCLUDED_ENDPOINT_H

#include "xapiand.h"

#include <string>


class Endpoint;
class Endpoints;


#ifdef HAVE_CXX11
#  include <unordered_set>
   typedef std::unordered_set<Endpoint> endpoints_set_t;
#else
#  include <set>
   typedef std::set<Endpoint> endpoints_set_t;
#endif


inline char *normalize_path(const char * src, char * dst);

namespace std {
	template<>
	struct hash<Endpoint> {
		size_t operator()(const Endpoint &e) const;
	};


	template<>
	struct hash<Endpoints> {
		size_t operator()(const Endpoints &e) const;
	};
}

bool operator == (Endpoint const& le, Endpoint const& re);
bool operator == (Endpoints const& le, Endpoints const& re);


class Endpoint {
	inline std::string slice_after(std::string &subject, std::string delimiter);
	inline std::string slice_before(std::string &subject, std::string delimiter);

public:
	int port;
	std::string protocol, user, password, host, path, search;

	Endpoint(const std::string &uri, const std::string &base_, int port_);
	std::string as_string() const;
	bool operator< (const Endpoint & other) const;
};


class Endpoints : public endpoints_set_t {
public:
	size_t hash(bool writable) const;
	std::string as_string() const;
};

#endif /* XAPIAND_INCLUDED_ENDPOINT_H */

