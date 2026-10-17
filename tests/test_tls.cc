/*
 * HTTPS (transport-security D1.5, D1.6): refusal counting, reactor's cap refusals, the
 * daily expiry check, and the expiry the context reports.
 */

#include "test_harness.h"

#ifdef XAPIAND_TLS

#include <chrono>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <sys/stat.h>

#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include "tls.h"

namespace {

const std::string dir = "test_tls.d";

// Writes a self-signed P-256 certificate valid from `from` to `to` seconds from now, and
// its key at `mode`; returns the certificate's path ({name}.pem, {name}.key).
std::string make_pair(const std::string& name, long from, long to, mode_t mode = 0600) {
	::mkdir(dir.c_str(), 0700);
	EVP_PKEY* key = EVP_EC_gen("P-256");
	X509* x = X509_new();
	X509_set_version(x, 2);
	ASN1_INTEGER_set(X509_get_serialNumber(x), 1);
	X509_gmtime_adj(X509_getm_notBefore(x), from);
	X509_gmtime_adj(X509_getm_notAfter(x), to);
	X509_set_pubkey(x, key);
	X509_NAME* subject = X509_NAME_new();
	X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("localhost"), -1, -1, 0);
	X509_set_subject_name(x, subject);
	X509_set_issuer_name(x, subject);
	X509_NAME_free(subject);
	X509_sign(x, key, EVP_sha256());
	const std::string cert = dir + "/" + name + ".pem", keyfile = dir + "/" + name + ".key";
	FILE* f = std::fopen(cert.c_str(), "w");
	PEM_write_X509(f, x);
	std::fclose(f);
	f = std::fopen(keyfile.c_str(), "w");
	PEM_write_PrivateKey(f, key, nullptr, nullptr, 0, nullptr, nullptr);
	std::fclose(f);
	::chmod(keyfile.c_str(), mode);
	X509_free(x);
	EVP_PKEY_free(key);
	return cert;
}

std::string key_of(const std::string& cert) { return cert.substr(0, cert.size() - 4) + ".key"; }

bool refused(const std::string& cert, const std::string& reason) {
	try {
		tls_setup(cert, key_of(cert), 64);
	} catch (const std::runtime_error& e) {
		return std::string(e.what()).find(reason) != std::string::npos;
	}
	return false;
}

}  // namespace

static void test_refusals_counted_per_reason() {
	const auto before = tls_refusals(TlsRefusal::protocol);
	tls_refused(TlsRefusal::protocol);
	tls_refused(TlsRefusal::protocol, 2);
	CHECK_EQ(tls_refusals(TlsRefusal::protocol), before + 3);
	CHECK_EQ(tls_refusals(TlsRefusal::cipher), 0u);
	CHECK_EQ(std::string(tls_refusal_name(TlsRefusal::cap)), "cap");
	CHECK_EQ(std::string(tls_refusal_name(TlsRefusal::deadline)), "deadline");
}

static void test_watch_counts_rejected_as_cap() {
	TlsWatch watch;
	const auto t0 = std::chrono::steady_clock::now();
	const auto cap = tls_refusals(TlsRefusal::cap);
	watch.tick(t0, 0);
	CHECK_EQ(tls_refusals(TlsRefusal::cap), cap);
	watch.tick(t0 + std::chrono::seconds(1), 5);
	CHECK_EQ(tls_refusals(TlsRefusal::cap), cap + 5);
	watch.tick(t0 + std::chrono::seconds(2), 5);  // no new rejections
	CHECK_EQ(tls_refusals(TlsRefusal::cap), cap + 5);
	watch.tick(t0 + std::chrono::seconds(3), 7);
	CHECK_EQ(tls_refusals(TlsRefusal::cap), cap + 7);
}

static void test_watch_checks_expiry_daily() {
	using std::chrono::hours;
	TlsWatch watch;
	const auto t0 = std::chrono::steady_clock::now();
	watch.tick(t0, 0);
	CHECK_EQ(watch.expiry_checks(), 0u);  // startup's check is tls_setup()'s
	watch.tick(t0 + hours(23), 0);
	CHECK_EQ(watch.expiry_checks(), 0u);
	watch.tick(t0 + hours(24), 0);
	CHECK_EQ(watch.expiry_checks(), 1u);
	watch.tick(t0 + hours(25), 0);
	CHECK_EQ(watch.expiry_checks(), 1u);
	watch.tick(t0 + hours(48), 0);
	CHECK_EQ(watch.expiry_checks(), 2u);
}

static void test_expiry_seconds() {
	const auto soon = make_pair("soon", -3600, 3 * 86400);
	tls_setup(soon, key_of(soon), 64);
	const long left = tls_seconds_to_expiry();
	CHECK_GT(left, 3 * 86400 - 60);
	CHECK_LE(left, 3 * 86400);
	CHECK_LT(tls_check_expiry(), 14 * 86400);  // inside the warning window
	const auto later = make_pair("later", -3600, 30 * 86400);
	tls_setup(later, key_of(later), 64);
	CHECK_GT(tls_check_expiry(), 29 * 86400);
}

static void test_setup_refusals() {
	CHECK(refused(make_pair("expired", -2 * 86400, -86400), "has expired"));
	CHECK(refused(make_pair("future", 86400, 2 * 86400), "is not valid yet"));
	CHECK(refused(make_pair("readable", -3600, 86400, 0640), "make it mode 0600"));
}

TEST_MAIN(
	test_refusals_counted_per_reason,
	test_watch_counts_rejected_as_cap,
	test_watch_checks_expiry_daily,
	test_expiry_seconds,
	test_setup_refusals
)

#else

int main() { return 0; }

#endif
