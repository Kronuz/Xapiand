/*
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

#include "exception.h"                            // for Exception
#include "hashes.hh"                              // for hhl
#include "log.h"                                  // for L_EXC
#include "phf.hh"                                 // for phf::*
#include "http_parser.h"                          // for HTTP_STATUS_*


struct http_errors_t {
	enum http_status error_code;
	std::string error;
	int ret;

	http_errors_t() : error_code{HTTP_STATUS_INTERNAL_SERVER_ERROR}, ret{1} {}
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
	} catch (const Xapian::DocNotFoundError&) {
		http_errors.error_code = HTTP_STATUS_NOT_FOUND;
		http_errors.error = http_status_str(http_errors.error_code);
	} catch (const Xapian::DatabaseNotFoundError&) {
		http_errors.error_code = HTTP_STATUS_NOT_FOUND;
		http_errors.error = http_status_str(http_errors.error_code);
	} catch (const Xapian::DocVersionConflictError& exc) {
		http_errors.error_code = HTTP_STATUS_CONFLICT;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.get_msg();
	} catch (const Xapian::DatabaseNotAvailableError& exc) {
		http_errors.error_code = HTTP_STATUS_SERVICE_UNAVAILABLE;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.get_msg();
	} catch (const Xapian::NetworkTimeoutError& exc) {
		http_errors.error_code = HTTP_STATUS_GATEWAY_TIMEOUT;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.get_msg();
	} catch (const Xapian::DatabaseModifiedError& exc) {
		http_errors.error_code = HTTP_STATUS_SERVICE_UNAVAILABLE;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.get_msg();
	} catch (const Xapian::NetworkError& exc) {
		std::string msg;
		const char* error_string = exc.get_error_string();
		if (!error_string) {
			msg = exc.get_msg();
			error_string = msg.c_str();
		}
		constexpr static auto _ = phf::make_phf({
			hhl("Can't assign requested address"),
			hhl("Connection refused"),
			hhl("Connection reset by peer"),
			hhl("Connection closed unexpectedly"),
		});
		switch (_.fhhl(error_string)) {
			case _.fhhl("Endpoint node not available"):
				http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
				http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
				break;
			case _.fhhl("Can't assign requested address"):
				http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
				http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
				break;
			case _.fhhl("Connection refused"):
				http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
				http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
				break;
			case _.fhhl("Connection reset by peer"):
				http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
				http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
				break;
			case _.fhhl("Connection closed unexpectedly"):
				http_errors.error_code = HTTP_STATUS_BAD_GATEWAY;
				http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + error_string;
				break;
			default:
				http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				http_errors.error = exc.get_description();
				L_EXC("ERROR: Dispatching HTTP request");
		}
	} catch (const ClientError& exc) {
		http_errors.error_code = HTTP_STATUS_BAD_REQUEST;
		http_errors.error = std::string(http_status_str(http_errors.error_code)) + ": " + exc.what();
	} catch (const BaseException& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = *exc.get_message() != 0 ? exc.get_message() : "Unkown BaseException!";
		L_EXC("ERROR: Dispatching HTTP request");
	} catch (const Xapian::Error& exc) {
		http_errors.error_code = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		http_errors.error = exc.get_description();
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
