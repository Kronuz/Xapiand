/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <string>                                 // std::string

#include "cartesian.h"                            // for CartesianError
#include "datetime.h"                             // for DatetimeError
#include "engine/error.h"                         // for engine::Error, engine::errc
#include "xapiand_exception.h"                            // for Exception
#include "hashes.hh"                              // for hhl
#include "log.h"                                  // for L_EXC
#include "phf.hh"                                 // for phf::*
#include "http_parser.h"                          // for HTTP_STATUS_*


struct http_errors_t {
	enum http_status error_code;
	std::string error;
	int ret;

	http_errors_t() : error_code(HTTP_STATUS_INTERNAL_SERVER_ERROR), ret(1) {}
};


template <typename Func>
http_errors_t
catch_http_errors(Func&& func)
{
	http_errors_t http_errors;
	try {
		http_errors.ret = func();
		http_errors.error_code = HTTP_STATUS_OK;
	} catch (const MissingTypeError& exc) {
		http_errors.error_code = HTTP_STATUS_PRECONDITION_FAILED;
		http_errors.error = exc.what();
	} catch (const engine::Error& exc) {
		const auto status = [&](enum http_status code) {
			http_errors.error_code = code;
			http_errors.error = std::string(http_status_str(code)) + ": " + exc.reason();
		};
		switch (exc.kind) {
			case engine::errc::doc_not_found:
			case engine::errc::not_found:
				status(HTTP_STATUS_NOT_FOUND);
				break;
			case engine::errc::version_conflict:
				status(HTTP_STATUS_CONFLICT);
				break;
			case engine::errc::not_available:
			case engine::errc::reopen:
				status(HTTP_STATUS_SERVICE_UNAVAILABLE);
				break;
			case engine::errc::timeout:
				status(HTTP_STATUS_GATEWAY_TIMEOUT);
				break;
			case engine::errc::network: {
				const auto& error_string = exc.detail().empty() ? exc.reason() : exc.detail();
				constexpr static auto _ = phf::make_phf({
					hhl("Can't assign requested address"),
					hhl("Connection refused"),
					hhl("Connection reset by peer"),
					hhl("Connection closed unexpectedly"),
				});
				switch (_.fhhl(error_string.c_str())) {
					case _.fhhl("Can't assign requested address"):
					case _.fhhl("Connection refused"):
					case _.fhhl("Connection reset by peer"):
					case _.fhhl("Connection closed unexpectedly"):
						http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
						http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
						break;
					default:
						http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
						http_errors.error = exc.description();
						L_EXC("ERROR: Dispatching HTTP request");
				}
				break;
			}
			default:
				if (const auto* client_error = dynamic_cast<const ClientError*>(&exc)) {
					http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
					http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + client_error->what();
				} else if (exc.kind == engine::errc::syntax) {
					http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
					http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.description();
				} else {
					http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
					http_errors.error = exc.description();
					L_EXC("ERROR: Dispatching HTTP request");
				}
		}
	} catch (const DatetimeError& exc) {
		// DatetimeError is now std::runtime_error-based (it comes from the
		// standalone datetime library, no longer a ClientError), so catch it
		// explicitly to keep malformed dates a 400 rather than a 500.
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	} catch (const CartesianError& exc) {
		// CartesianError, like DatetimeError, comes from a standalone library
		// (cartesian) as a std::runtime_error subtype rather than a ClientError,
		// so catch it explicitly to keep a bad coordinate/SRID a 400 not a 500.
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	} catch (const ClientError& exc) {
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	} catch (const BaseException& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = *exc.get_message() != 0 ? exc.get_message() : "Unkown BaseException!";
		L_EXC("ERROR: Dispatching HTTP request");
	} catch (const std::exception& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = *exc.what() != 0 ? exc.what() : "Unkown std::exception!";
		L_EXC("ERROR: Dispatching HTTP request");
	} catch (...) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = "Unknown exception!";
		L_EXC("ERROR: Dispatching HTTP request");
	}

	return http_errors;
}
