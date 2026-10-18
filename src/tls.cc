/*
 * HTTPS for the REST API: building and checking the TLS context (tls.h).
 */

#include "tls.h"

#ifdef XAPIAND_TLS

#include <array>              // for std::array
#include <ctime>              // for std::time
#include <stdexcept>          // for std::runtime_error
#include <sys/resource.h>     // for getrlimit, RLIMIT_NOFILE
#include <sys/stat.h>         // for stat

#include <openssl/x509.h>

#include "log.h"              // for L_WARNING, L_WARNING_ONCE_PER_MINUTE
#include "metrics.h"          // for Metrics::metrics
#include "reactor_tls.h"      // for reactor::tls::configure

std::shared_ptr<asio::ssl::context> tls_context;

namespace {

// The certificate the context presents.
X509* presented(const asio::ssl::context& context) {
	return SSL_CTX_get0_certificate(const_cast<asio::ssl::context&>(context).native_handle());
}

}  // namespace

const char* tls_key_mode_refusal(mode_t mode, gid_t group, gid_t runs_as) {
	if (mode & S_IRWXO) { return "is accessible by others"; }
	if (mode & 07777 & ~mode_t(0640)) { return "has mode bits outside 0640"; }
	if ((mode & S_IRGRP) && group != runs_as) { return "is readable by a group other than the one the node runs as"; }
	return nullptr;
}

void tls_setup(const std::string& cert, const std::string& key, std::size_t max_connections, gid_t runs_as) {
	struct stat st;
	if (::stat(key.c_str(), &st) != 0) { throw std::runtime_error("cannot read --tls-key " + key); }
	if (const char* why = tls_key_mode_refusal(st.st_mode, st.st_gid, runs_as)) {
		throw std::runtime_error("--tls-key " + key + " " + why + "; make it mode 0600, or 0640 with group " + std::to_string(runs_as));
	}
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
	const std::time_t now = std::time(nullptr);
	const int from = ASN1_TIME_cmp_time_t(X509_get0_notBefore(x), now), until = ASN1_TIME_cmp_time_t(X509_get0_notAfter(x), now);
	if (from == -2 || until == -2) { throw std::runtime_error("--tls-cert " + cert + " has an unreadable validity period"); }  // -2: error
	if (from > 0) { throw std::runtime_error("--tls-cert " + cert + " is not valid yet"); }
	if (until < 0) { throw std::runtime_error("--tls-cert " + cert + " has expired"); }
	tls_context = std::move(context);
	tls_check_expiry();
}

long tls_check_expiry() {
	const long left = tls_seconds_to_expiry();
	if (tls_context && left < 14 * 86400) {
		L_WARNING("The TLS certificate expires in {} days", left / 86400);
	}
	return left;
}

long tls_seconds_to_expiry() {
	if (!tls_context) { return 0; }
	X509* x = presented(*tls_context);
	int days = 0, seconds = 0;
	if (!x || ASN1_TIME_diff(&days, &seconds, nullptr, X509_get0_notAfter(x)) != 1) { return 0; }
	return long(days) * 86400 + seconds;
}

namespace {

// The metric's counter for each reason, made once.
prometheus::Counter& refused_counter(TlsRefusal why) {
	static const auto counters = [] {
		std::array<prometheus::Counter*, tls_refusal_kinds> made{};
		auto& family = Metrics::metrics().xapiand_tls_refused_connections;
		for (std::size_t i = 0; i < made.size(); ++i) {
			made[i] = &family.Add({{"reason", tls_refusal_name(static_cast<TlsRefusal>(i))}});
		}
		return made;
	}();
	return *counters[static_cast<std::size_t>(why)];
}

}  // namespace

const char* tls_refusal_name(TlsRefusal why) {
	static constexpr const char* names[tls_refusal_kinds] = {
		"deadline", "connections", "handshakes", "version", "cipher", "certificate", "protocol", "other", "cap"};
	return names[static_cast<std::size_t>(why)];
}

void tls_refused(TlsRefusal why, std::uint64_t count) {
	refused_counter(why).Increment(static_cast<double>(count));
	switch (why) {  // one call site per reason, so each is rate-limited on its own
		case TlsRefusal::deadline: L_WARNING_ONCE_PER_MINUTE("TLS handshake past its deadline"); break;
		case TlsRefusal::connections: L_WARNING_ONCE_PER_MINUTE("TLS connection refused: --tls-max-connections reached"); break;
		case TlsRefusal::handshakes: L_WARNING_ONCE_PER_MINUTE("TLS connection refused: too many handshakes in progress"); break;
		case TlsRefusal::version: L_WARNING_ONCE_PER_MINUTE("TLS handshake refused: protocol version"); break;
		case TlsRefusal::cipher: L_WARNING_ONCE_PER_MINUTE("TLS handshake refused: no shared cipher"); break;
		case TlsRefusal::certificate: L_WARNING_ONCE_PER_MINUTE("TLS handshake failed: the client refused the certificate"); break;
		case TlsRefusal::protocol: L_WARNING_ONCE_PER_MINUTE("TLS port received something other than TLS (plain HTTP?)"); break;
		case TlsRefusal::other: L_WARNING_ONCE_PER_MINUTE("TLS handshake failed"); break;
		case TlsRefusal::cap: L_WARNING_ONCE_PER_MINUTE("TLS connection refused at accept: --tls-max-connections reached"); break;
	}
}

std::uint64_t tls_refusals(TlsRefusal why) {
	return static_cast<std::uint64_t>(refused_counter(why).Value());
}

void TlsWatch::tick(std::chrono::steady_clock::time_point now, std::uint64_t rejected_total) {
	if (rejected_total > rejected) {
		tls_refused(TlsRefusal::cap, rejected_total - rejected);
		rejected = rejected_total;
	}
	if (next == std::chrono::steady_clock::time_point{}) {
		next = now + period;  // tls_setup() checked at startup
	} else if (now >= next) {
		tls_check_expiry();
		next = now + period;
		++checks;
	}
}

#endif
