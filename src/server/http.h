/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#pragma once

#include "config.h"                           // for XAPIAND_HTTP_PROTOCOL_MAJOR_VERSION, XAPIAND_HTTP_PROTOCOL_MINOR_VERSION

#include <stdio.h>                            // for snprintf
#include <memory>                             // for std::shared_ptr, std::weak_ptr
#include <string>                             // for std::string
#include <vector>                             // for std::vector

#include "tcp.h"                              // for BaseTCP

class HttpServer;


constexpr uint16_t XAPIAND_HTTP_PROTOCOL_MAJOR_VERSION = 1;
constexpr uint16_t XAPIAND_HTTP_PROTOCOL_MINOR_VERSION = 1;


// Configuration data for Http
class Http : public BaseTCP {
	friend HttpServer;

public:
	Http(const std::shared_ptr<Worker>& parent_, ev::loop_ref* ev_loop_, unsigned int ev_flags_, int port_);

	std::string getDescription() const;

	void start();

	std::string __repr__() const override;
};
