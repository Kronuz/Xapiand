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

#include "http.h"

#include "manager.h"

#include <assert.h>


Http::Http(const std::shared_ptr<XapiandManager>& manager_, int port_)
	: BaseTCP(manager_, port_, "Http", port_ == XAPIAND_HTTP_SERVERPORT ? 10 : 1, CONN_TCP_NODELAY | CONN_TCP_DEFER_ACCEPT)
{
	local_node.http_port = port;

	L_OBJ(this, "CREATED CONFIGURATION FOR HTTP [%llx]", this);
}


Http::~Http()
{
	L_OBJ(this, "DELETED CONFIGURATION FOR HTTP [%llx]", this);
}


std::string
Http::getDescription() const noexcept
{
	return "TCP:" + std::to_string(port) + " (" + description + ")";
}
