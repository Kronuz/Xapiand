/*
 * HTTPS for the REST API: the TLS context, built before the process drops privileges
 * (docs/design/transport-security.md D1).
 */

#pragma once

#ifdef XAPIAND_TLS

#include <chrono>       // for std::chrono
#include <cstddef>      // for std::size_t
#include <cstdint>      // for std::uint64_t
#include <memory>       // for std::shared_ptr
#include <string>       // for std::string
#include <sys/types.h>  // for mode_t, gid_t

#include <asio/ssl.hpp>

// The API's TLS context, built by tls_setup() before the process drops privileges; null
// when the API serves plain HTTP.
extern std::shared_ptr<asio::ssl::context> tls_context;

// Why a key file of this mode and group is refused, or nullptr (SYS-13): no bit outside
// 0640, and group read only when the file's group is `runs_as`, the group the node runs as.
const char* tls_key_mode_refusal(mode_t mode, gid_t group, gid_t runs_as);

// Builds tls_context from --tls-cert and --tls-key (reactor's Server role, strict), after
// checking the key file's mode (tls_key_mode_refusal against `runs_as`: the group demote()
// switches to, or the effective group), that the key matches the certificate, that the
// certificate is valid now, and that --tls-max-connections fits the open-files limit.
// Throws std::runtime_error naming the reason; warns within 14 days of expiry.
void tls_setup(const std::string& cert, const std::string& key, std::size_t max_connections, gid_t runs_as);

// Seconds until the certificate expires (negative once expired), for the daily check.
long tls_seconds_to_expiry();

// Warns when the certificate expires within 14 days; returns the seconds left.
long tls_check_expiry();

// Why a TLS connection was refused: http's handshake-failure reasons, plus cap for the
// connections reactor refuses at accept once --tls-max-connections are open (D1.5).
enum class TlsRefusal { deadline, connections, handshakes, version, cipher, certificate, protocol, other, cap };
inline constexpr std::size_t tls_refusal_kinds = 9;
const char* tls_refusal_name(TlsRefusal why);

// Counts `count` refusals (the xapiand_tls_refused_connections metric) and logs at most
// once a minute per reason.
void tls_refused(TlsRefusal why, std::uint64_t count = 1);
std::uint64_t tls_refusals(TlsRefusal why);

// The control-plane check, ticked every second: turns reactor's cumulative rejected count
// into cap refusals, and checks expiry every `period` after its first tick (tls_setup()
// checks at startup).
class TlsWatch {
	std::chrono::seconds period;
	std::chrono::steady_clock::time_point next{};
	std::uint64_t rejected = 0;
	std::size_t checks = 0;

public:
	explicit TlsWatch(std::chrono::seconds period_ = std::chrono::hours(24)) : period(period_) {}
	void tick(std::chrono::steady_clock::time_point now, std::uint64_t rejected_total);
	std::size_t expiry_checks() const noexcept { return checks; }
};

#endif
