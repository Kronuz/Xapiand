/*
 * HTTPS (transport-security D1.5, D1.6): refusal counting, reactor's cap refusals, the
 * daily expiry check, the expiry the context reports, and the key-file modes against the
 * group the node runs as (SYS-13).
 */

#include "test_harness.h"

#ifdef XAPIAND_TLS

#include <chrono>
#include <cstdio>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

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

// Sets the key's group (its owner may give it any group it belongs to).
void set_group(const std::string& cert, gid_t group) { CHECK_EQ(::chown(key_of(cert).c_str(), uid_t(-1), group), 0); }

// A group this user belongs to besides its effective one, or nullopt.
std::optional<gid_t> second_group() {
	std::vector<gid_t> groups(std::size_t(::getgroups(0, nullptr)));
	groups.resize(std::size_t(::getgroups(int(groups.size()), groups.data())));
	for (gid_t group : groups) {
		if (group != ::getegid()) { return group; }
	}
	return std::nullopt;
}

// Whether tls_setup() refuses, naming `reason`; the node runs as `runs_as`.
bool refused(const std::string& cert, const std::string& reason, gid_t runs_as = ::getegid()) {
	try {
		tls_setup(cert, key_of(cert), 64, runs_as);
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
	tls_setup(soon, key_of(soon), 64, ::getegid());
	const long left = tls_seconds_to_expiry();
	CHECK_GT(left, 3 * 86400 - 60);
	CHECK_LE(left, 3 * 86400);
	CHECK_LT(tls_check_expiry(), 14 * 86400);  // inside the warning window
	const auto later = make_pair("later", -3600, 30 * 86400);
	tls_setup(later, key_of(later), 64, ::getegid());
	CHECK_GT(tls_check_expiry(), 29 * 86400);
}

static void test_setup_refusals() {
	CHECK(refused(make_pair("expired", -2 * 86400, -86400), "has expired"));
	CHECK(refused(make_pair("future", 86400, 2 * 86400), "is not valid yet"));
	CHECK(refused(make_pair("readable", -3600, 86400, 0644), "make it mode 0600"));
}

static void test_key_mode_rule() {
	const gid_t own = 20, other = 21;
	auto refusal = [&](mode_t mode, gid_t group) {
		const char* why = tls_key_mode_refusal(mode, group, own);
		return std::string(why ? why : "");
	};
	for (mode_t mode : std::initializer_list<mode_t>{0600, 0400, 0640, 0440, 0040}) {
		CHECK_EQ(refusal(mode, own), "");
	}
	CHECK_EQ(refusal(0600, other), "");  // no group bit, so the group does not matter
	CHECK_EQ(refusal(0640, other), "is readable by a group other than the one the node runs as");
	CHECK_EQ(refusal(0440, other), "is readable by a group other than the one the node runs as");
	for (mode_t mode : std::initializer_list<mode_t>{0660, 0650, 0620, 0610, 0700, 0740, 04600, 02640, 01600}) {
		CHECK_EQ(refusal(mode, own), "has mode bits outside 0640");
	}
	for (mode_t mode : std::initializer_list<mode_t>{0644, 0604, 0602, 0601, 0666}) {
		CHECK_EQ(refusal(mode, own), "is accessible by others");
	}
	CHECK_EQ(refusal(S_IFREG | 0640, own), "");  // the file type bits from stat() are ignored
}

static void test_setup_key_modes() {
	const auto own = make_pair("own_group", -3600, 86400, 0640);
	set_group(own, ::getegid());
	CHECK(!refused(own, ""));  // 0640 with the effective group starts
	const auto writable = make_pair("group_writable", -3600, 86400, 0660);
	set_group(writable, ::getegid());
	CHECK(refused(writable, "has mode bits outside 0640"));
	if (const auto group = second_group()) {
		const auto foreign = make_pair("other_group", -3600, 86400, 0640);
		set_group(foreign, *group);
		CHECK(refused(foreign, "other than the one the node runs as"));
	} else {
		std::fprintf(stderr, "  (no second group: the other-group file case is covered by test_key_mode_rule only)\n");
	}
}

// A root start with --gid: the key is checked against the --gid group, not the group the
// process starts with, so a key at 0640 with root's group is refused (SYS-13).
static void test_root_start_with_gid() {
	const gid_t root = 0, target = 1001;
	CHECK(tls_key_mode_refusal(0640, root, target) != nullptr);
	CHECK(tls_key_mode_refusal(0440, root, target) != nullptr);
	CHECK(tls_key_mode_refusal(0640, target, target) == nullptr);
	CHECK(tls_key_mode_refusal(0600, root, target) == nullptr);
	// The same through tls_setup(): the key carries the starting group, the node runs as another.
	const gid_t starting = ::getegid(), runs_as = starting + 1;
	const auto start_group = make_pair("start_group", -3600, 86400, 0640);
	set_group(start_group, starting);
	CHECK(refused(start_group, "other than the one the node runs as", runs_as));
	CHECK(refused(start_group, "0640 with group " + std::to_string(runs_as), runs_as));
	CHECK(!refused(start_group, "", starting));  // the same key passes when the node keeps that group
	// The key carrying the --gid group passes, given a second group to set it to.
	if (const auto group = second_group()) {
		const auto gid_group = make_pair("gid_group", -3600, 86400, 0640);
		set_group(gid_group, *group);
		CHECK(!refused(gid_group, "", *group));
	} else {
		std::fprintf(stderr, "  (no second group: the --gid-group file case is covered by the rule checks only)\n");
	}
}

TEST_MAIN(
	test_refusals_counted_per_reason,
	test_watch_counts_rejected_as_cap,
	test_watch_checks_expiry_daily,
	test_expiry_seconds,
	test_setup_refusals,
	test_key_mode_rule,
	test_setup_key_modes,
	test_root_start_with_gid
)

#else

int main() { return 0; }

#endif
