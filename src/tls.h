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
