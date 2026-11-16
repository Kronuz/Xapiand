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
 * HTTPS for the REST API: building and checking the TLS context (tls.h).
 */

#include "tls.h"

#ifdef XAPIAND_TLS

#include <stdexcept>          // for std::runtime_error
#include <sys/resource.h>     // for getrlimit, RLIMIT_NOFILE
#include <sys/stat.h>         // for stat

#include <openssl/x509.h>

#include "log.h"              // for L_WARNING
#include "reactor_tls.h"      // for reactor::tls::configure

std::shared_ptr<asio::ssl::context> tls_context;

namespace {

// The certificate the context presents.
X509* presented(const asio::ssl::context& context) {
	return SSL_CTX_get0_certificate(const_cast<asio::ssl::context&>(context).native_handle());
}

}  // namespace

void tls_setup(const std::string& cert, const std::string& key, std::size_t max_connections) {
	struct stat st;
	if (::stat(key.c_str(), &st) != 0) { throw std::runtime_error("cannot read --tls-key " + key); }
	if (st.st_mode & (S_IRWXG | S_IRWXO)) { throw std::runtime_error("--tls-key " + key + " is readable by group or others; make it mode 0600"); }
	struct rlimit files;
	if (::getrlimit(RLIMIT_NOFILE, &files) == 0 && files.rlim_cur != RLIM_INFINITY && max_connections + 64 > files.rlim_cur) {
		throw std::runtime_error("--tls-max-connections " + std::to_string(max_connections) + " does not fit the open-files limit " +
			std::to_string(files.rlim_cur) + " (it needs 64 to spare)");
	}
	auto context = std::make_shared<asio::ssl::context>(asio::ssl::context::tls);
	reactor::tls::Options options{cert, key, ""};
	options.strict = true;
	try {
		reactor::tls::configure(*context, options, reactor::tls::Role::Server);  // checks the key matches the certificate
	} catch (const std::exception& e) {
		throw std::runtime_error(std::string("TLS certificate or key: ") + e.what());
	}
	X509* x = presented(*context);
	if (!x) { throw std::runtime_error("--tls-cert " + cert + " holds no certificate"); }
	if (X509_cmp_current_time(X509_get0_notBefore(x)) > 0) { throw std::runtime_error("--tls-cert " + cert + " is not valid yet"); }
	if (X509_cmp_current_time(X509_get0_notAfter(x)) < 0) { throw std::runtime_error("--tls-cert " + cert + " has expired"); }
	tls_context = std::move(context);
	if (const long left = tls_seconds_to_expiry(); left < 14 * 86400) {
		L_WARNING("The TLS certificate expires in {} days", left / 86400);
	}
}

long tls_seconds_to_expiry() {
	if (!tls_context) { return 0; }
	X509* x = presented(*tls_context);
	int days = 0, seconds = 0;
	if (!x || ASN1_TIME_diff(&days, &seconds, nullptr, X509_get0_notAfter(x)) != 1) { return 0; }
	return long(days) * 86400 + seconds;
}

#endif
