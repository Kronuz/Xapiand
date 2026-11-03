/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

/*
 * HTTPS for the REST API: the TLS context, built before the process drops privileges
 * (docs/design/transport-security.md D1).
 */

#pragma once

#ifdef XAPIAND_TLS

#include <cstddef>      // for std::size_t
#include <memory>       // for std::shared_ptr
#include <string>       // for std::string

#include <asio/ssl.hpp>

// The API's TLS context, built by tls_setup() before the process drops privileges; null
// when the API serves plain HTTP.
extern std::shared_ptr<asio::ssl::context> tls_context;

// Builds tls_context from --tls-cert and --tls-key (reactor's Server role, strict), after
// checking that the key file is readable by its owner only, that the key matches the
// certificate, that the certificate is valid now, and that --tls-max-connections fits the
// open-files limit. Throws std::runtime_error naming the reason; warns within 14 days of
// expiry.
void tls_setup(const std::string& cert, const std::string& key, std::size_t max_connections);

// Seconds until the certificate expires (negative once expired), for the daily check.
long tls_seconds_to_expiry();

#endif
