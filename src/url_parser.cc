/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "url_parser.h"

#include <cstring>           // for strlen, strncmp

#include "database_utils.h"  // for normalize_uuid
#include "manager.h"         // for XapiandManager::manager->opts.uuid_partition
#include "utils.h"           // for hexdec

std::string
urldecode(const void *p, size_t size)
{
	std::string buf;
	buf.reserve(size);
	const char* q = (const char *)p;
	auto p_end = q + size;
	while (q != p_end) {
		char c = *q++;
		switch (c) {
			case '+':
				buf.push_back(' ');
				break;
			case '%': {
				auto dec = hexdec(&q);
				if (dec != -1) {
					c = dec;
				}
			}
			default:
				buf.push_back(c);
		}
	}
	return buf;
}


std::string
urlexpand(const void *p, size_t size, std::string(*fn)(const std::string&))
{
	std::string buf;
	buf.reserve(size);
	const char* q = (const char *)p;
	auto p_end = q + size;
	auto oq = q;
	while (q != p_end) {
		char c = *q++;
		switch (c) {
			case '+':
				buf.push_back(' ');
				break;
			case '/': {
				auto sz = q - oq - 1;
				if (sz) {
					try {
						auto uuid_normalized = fn(std::string(oq, sz));
						buf.resize(buf.size() - sz);
						buf.append(uuid_normalized);
					} catch (const SerialisationError& exc) { }
				}
				buf.push_back(c);
				oq = q;
				break;
			}
			case '%': {
				auto dec = hexdec(&q);
				if (dec != -1) {
					c = dec;
				}
			}
			default:
				buf.push_back(c);
		}
	}
	auto sz = q - oq;
	if (sz) {
		try {
			auto uuid_normalized = fn(std::string(oq, sz));
			buf.resize(buf.size() - sz);
			buf.append(uuid_normalized);
		} catch (const SerialisationError& exc) { }
	}
	return buf;
}


QueryParser::QueryParser()
	: len(0),
	  off(nullptr) { }


void
QueryParser::clear() noexcept
{
	rewind();
	query.clear();
}


void
QueryParser::rewind() noexcept
{
	len = 0;
	off = nullptr;
}


int
QueryParser::init(const std::string& q)
{
	clear();
	query = q;
	return 0;
}


int
QueryParser::next(const char *name)
{
	const char *ni = query.data();
	const char *nf = ni + query.length();
	const char *n0, *n1 = nullptr;
	const char *v0 = nullptr;

	if (off == nullptr) {
		n0 = n1 = ni;
	} else {
		n0 = n1 = off + len;
	}

	while (true) {
		char cn = *n1;
		if (n1 == nf) {
			cn = '\0';
		}
		switch (cn) {
			case '=' :
				v0 = n1;
			case '\0':
			case '&' :
			case ';' :
				if (strlen(name) == static_cast<size_t>(n1 - n0) && strncmp(n0, name, n1 - n0) == 0) {
					if (v0) {
						const char *v1 = v0 + 1;
						while (1) {
							char cv = *v1;
							if (v1 == nf) {
								cv = '\0';
							}
							switch(cv) {
								case '\0':
								case '&' :
								case ';' :
								off = v0 + 1;
								len = v1 - v0 - 1;
								return 0;
							}
							++v1;
						}
					} else {
						off = n1 + 1;
						len = 0;
						return 0;
					}
				} else if (!cn) {
					return -1;
				} else if (cn != '=') {
					n0 = n1 + 1;
					v0 = nullptr;
				}
		}
		++n1;
	}

	return -1;
}


std::string
QueryParser::get()
{
	if (!off) return std::string();
	return urldecode(off, len);
}


PathParser::PathParser()
	: off(nullptr), len_pth(0),
	  off_pth(nullptr), len_hst(0),
	  off_hst(nullptr), len_nsp(0),
	  off_nsp(nullptr), len_pmt(0),
	  off_pmt(nullptr), len_ppmt(0),
	  off_ppmt(nullptr), len_cmd(0),
	  off_cmd(nullptr), len_id(0),
	  off_id(nullptr) { }


void
PathParser::clear() noexcept
{
	rewind();
	len_pmt = 0;
	off_pmt = nullptr;
	len_ppmt = 0;
	off_ppmt = nullptr;
	len_cmd = 0;
	off_cmd = nullptr;
	len_id = 0;
	off_id = nullptr;
	path.clear();
}


void
PathParser::rewind() noexcept
{
	off = path.data();
	len_pth = 0;
	off_pth = nullptr;
	len_hst = 0;
	off_hst = nullptr;
	len_nsp = 0;
	off_nsp = nullptr;
}


PathParser::State
PathParser::init(const std::string& p)
{
	clear();
	path = p;

	char cn;
	size_t length;
	const char *ni = path.data();
	const char *nf = ni + path.length();
	const char *n0, *n1 = nullptr;
	State state;

	state = State::NCM;

	// First figure out entry point (if it has a command)
	n0 = n1 = ni;

	cn = '\xff';
	while (cn) {
		cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
		#ifdef DEBUG_URL_PARSER
		fprintf(stderr, "1>> %3s %02x '%c' [n1:%td - n0:%td = length:%td] total:%td\n", [state]{
			switch(state) {
				case State::NCM: return "ncm";
				case State::PMT: return "pmt";
				case State::CMD: return "cmd";
				case State::ID: return "id";
				case State::NSP: return "nsp";
				case State::PTH: return "pth";
				case State::HST: return "hst";
				case State::END: return "end";
				case State::INVALID_STATE: return "INVALID_STATE";
				case State::INVALID_NSP: return "INVALID_NSP";
				case State::INVALID_HST: return "INVALID_HST";
			}
		}(), (int)cn, cn, n0 - ni, n1 - ni, n1 - n0, nf - ni);
		#endif

		switch (cn) {
			case '/':
				++n1;
				cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
				if (cn == '.') {
					state = State::CMD;
					cn = '\0';
				}
			default:
				break;
		}
		++n1;
	}

	// Then go backwards, looking for pmt, cmd and id
	// id is filled only if there's no pmt already:
	n0 = n1 = nf - 1;

	cn = '\xff';
	while (cn) {
		cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
		#ifdef DEBUG_URL_PARSER
		fprintf(stderr, "2<< %3s %02x '%c' [n1:%td - n0:%td = length:%td] total:%td\n", [state]{
			switch(state) {
				case State::NCM: return "ncm";
				case State::PMT: return "pmt";
				case State::CMD: return "cmd";
				case State::ID: return "id";
				case State::NSP: return "nsp";
				case State::PTH: return "pth";
				case State::HST: return "hst";
				case State::END: return "end";
				case State::INVALID_STATE: return "INVALID_STATE";
				case State::INVALID_NSP: return "INVALID_NSP";
				case State::INVALID_HST: return "INVALID_HST";
			}
		}(), (int)cn, cn, n1 - ni, n0 - ni, n0 - n1, nf - ni);
		#endif

		switch (cn) {
			case '\0':
			case '/':
				switch (state) {
					case State::NCM:
						state = State::ID;
						n0 = n1 - 1;
						break;

					case State::CMD:
						state = State::PMT;
						n0 = n1 - 1;
						break;

					case State::PMT:
						assert(n0 >= n1);
						length = n0 - n1;
						if (length) {
							if (*(n1 + 1) == '.') {
								off_cmd = n1 + 1;
								len_cmd = length;
								state = State::ID;
							} else {
								off_ppmt = off_pmt;
								if (len_ppmt) ++len_ppmt;
								len_ppmt += len_pmt;
								off_pmt = n1 + 1;
								len_pmt = length;
							}
						}
						n0 = n1 - 1;
						break;

					case State::ID:
						assert(n0 >= n1);
						length = n0 - n1;
						if (length) {
							off_id = n1 + 1;
							len_id = length;
							cn = '\0';
						}
						n0 = n1 - 1;
						break;

					default:
						break;
				}
				break;

			case ',':
			case ':':
			case '@':
				switch (state) {
					case State::NCM:
						state = State::ID;
						n0 = n1;
						break;
					case State::CMD:
						state = State::PMT;
						n0 = n1;
						break;
					case State::ID:
						cn = '\0';
					default:
						break;
				}
				break;

			default:
				switch (state) {
					case State::NCM:
						state = State::ID;
						n0 = n1;
						break;
					case State::CMD:
						state = State::PMT;
						n0 = n1;
						break;
					default:
						break;
				}
				break;
		}
		--n1;
	}


	off = ni;
	return state;
}


PathParser::State
PathParser::next()
{
	char cn;
	size_t length;
	const char *ni = path.data();
	const char *nf = ni + path.length();
	const char *n0, *n1 = nullptr;
	State state;

	// Then goes forward, looking for endpoints:
	state = State::NSP;
	off_hst = nullptr;
	n0 = n1 = off;
	if (off_ppmt && off_ppmt < nf) {
		nf = off_ppmt - 1;
	}
	if (off_pmt && off_pmt < nf) {
		nf = off_pmt - 1;
	}
	if (off_cmd && off_cmd < nf) {
		nf = off_cmd - 1;
	}
	if (off_id && off_id < nf) {
		nf = off_id - 1;
	}
	if (nf < ni) {
		nf = ni;
	}
	if (n1 > nf) {
		return State::END;
	}

	cn = '\xff';
	while (true) {
		cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
		#ifdef DEBUG_URL_PARSER
		fprintf(stderr, "3>> %3s %02x '%c' [n1:%td - n0:%td = length:%td] total:%td\n", [state]{
			switch(state) {
				case State::NCM: return "ncm";
				case State::PMT: return "pmt";
				case State::CMD: return "cmd";
				case State::ID: return "id";
				case State::NSP: return "nsp";
				case State::PTH: return "pth";
				case State::HST: return "hst";
				case State::END: return "end";
				case State::INVALID_STATE: return "INVALID_STATE";
				case State::INVALID_NSP: return "INVALID_NSP";
				case State::INVALID_HST: return "INVALID_HST";
			}
		}(), (int)cn, cn, n0 - ni, n1 - ni, n1 - n0, nf - ni);
		#endif

		switch (cn) {
			case '\0':
			case ',':
				switch (state) {
					case State::NSP:
					case State::PTH:
						assert(n1 >= n0);
						length = n1 - n0;
						off_pth = n0;
						len_pth = length;
						off = ++n1;
						return state;
					case State::HST:
						assert(n1 >= n0);
						length = n1 - n0;
						if (!length) {
							return State::INVALID_HST;
						}
						off_hst = n0;
						len_hst = length;
						off = ++n1;
						return state;
					default:
						break;
				}
				break;

			case ':':
				switch (state) {
					case State::NSP:
						assert(n1 >= n0);
						length = n1 - n0;
						if (!length) {
							return State::INVALID_NSP;
						}
						off_nsp = n0;
						len_nsp = length;
						state = State::PTH;
						n0 = n1 + 1;
						break;
					default:
						break;
				}
				break;

			case '@':
				switch (state) {
					case State::NSP:
					case State::PTH:
						assert(n1 >= n0);
						length = n1 - n0;
						off_pth = n0;
						len_pth = length;
						state = State::HST;
						n0 = n1 + 1;
						break;
					default:
						break;
				}
				break;

			default:
				break;
		}
		++n1;
	}

	return state;
}


std::string
PathParser::get_pth()
{
	if (!off_pth) return std::string();
	if (XapiandManager::manager->opts.uuid_partition) {
		return urlexpand(off_pth, len_pth, normalize_uuid_partition);
	}
	return urlexpand(off_pth, len_pth, normalize_uuid);
}


std::string
PathParser::get_hst()
{
	if (!off_hst) return std::string();
	return urldecode(off_hst, len_hst);
}


std::string
PathParser::get_nsp()
{
	if (!off_nsp) return std::string();
	if (XapiandManager::manager->opts.uuid_partition) {
		return urlexpand(off_nsp, len_nsp, normalize_uuid_partition);
	}
	return urlexpand(off_nsp, len_nsp, normalize_uuid);
}


std::string
PathParser::get_pmt()
{
	if (!off_pmt) return std::string();
	return urldecode(off_pmt, len_pmt + (off_ppmt ? 0 : len_ppmt));
}


std::string
PathParser::get_ppmt()
{
	if (!off_ppmt) return std::string();
	return urldecode(off_ppmt, len_ppmt - 1);
}


std::string
PathParser::get_cmd()
{
	if (!off_cmd) return std::string();
	return urldecode(off_cmd, len_cmd);
}


std::string
PathParser::get_id()
{
	if (!off_id) return std::string();
	return urlexpand(off_id, len_id, normalize_uuid);
}
