/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include <cstring>           // for strncmp

#include "utils.h"           // for hexdec
#include "log.h"


#ifndef L_URL_PARSER
#define L_URL_PARSER_DEFINED
#define L_URL_PARSER L_NOTHING
#endif


constexpr const char cmd_prefix = COMMAND_PREFIX[0];

std::string
urldecode(const void *p, size_t size, char plus, char amp, char colon, char eq)
{
	std::string buf;
	buf.reserve(size);
	const char* q = (const char *)p;
	auto p_end = q + size;
	while (q != p_end) {
		char c = *q++;
		switch (c) {
			case '+':
				buf.push_back(plus);
				break;
			case '&':
				buf.push_back(amp);
				break;
			case ';':
				buf.push_back(colon);
				break;
			case '=':
				buf.push_back(eq);
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
QueryParser::init(std::string_view q)
{
	clear();
	query = urldecode(q, ' ', '\0', '\0', '\1');
	return 0;
}


int
QueryParser::next(const char *name, size_t name_len)
{
	const char *ni = query.data();
	const char *nf = ni + query.size();
	const char *n0, *n1 = nullptr;
	const char *v0 = nullptr;

	if (off == nullptr) {
		n0 = n1 = ni;
	} else {
		n0 = n1 = off + len;
	}

	while (true) {
		char cn = (n1 == nf) ? '\0' : *n1;
		switch (cn) {
			case '\1':  // '='
				v0 = n1;
			case '\0':  // '\0' and '&'
				if (name_len == static_cast<size_t>(n1 - n0) && strncmp(n0, name, n1 - n0) == 0) {
					if (v0) {
						const char *v1 = v0 + 1;
						while (1) {
							char cv = (v1 == nf) ? '\0' : *v1;
							switch(cv) {
								case '\0':  // '\0' and '&'
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
				} else if (cn != '\1') {  // '='
					n0 = n1 + 1;
					v0 = nullptr;
				}
		}
		++n1;
	}

	return -1;
}


std::string_view
QueryParser::get()
{
	if (!off) return "";
	return std::string_view(off, len);
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
PathParser::init(std::string_view p)
{
	clear();
	path = urldecode(p);

	L_URL_PARSER(repr(path));

	char cn, cn1, cn2, cp1;
	size_t length;
	const char *ni = path.data();
	const char *nf = ni + path.size();
	const char *n0, *n1 = nullptr;
	State state;

	state = State::NCM;

	// First figure out entry point (if it has a command)
	n0 = n1 = ni;

	cn = '\xff';
	while (cn) {
		cn = (n1 >= nf || n1 < ni) ? '\0' : *n1;
		L_URL_PARSER(GREEN + "1 ->> %3s 0x%02x '%c' [n1:%td - n0:%td = length:%td] total:%td", [state]{
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

		switch (cn) {
			case '/':
				cn1 = ((n1 + 1) >= nf || (n1 + 1) < ni) ? '\0' : *(n1 + 1);
				cn2 = ((n1 + 2) >= nf || (n1 + 2) < ni) ? '\0' : *(n1 + 2);
				if (cn1 == cmd_prefix && (cn2 == '_' || (cn2 >= 'A' && cn2 <= 'Z') || (cn2 >= 'a' && cn2 <= 'z'))) {
					state = State::CMD;
					cn1 = '\0';
					++n1;
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
		L_URL_PARSER(BLUE + "2 <<- %3s 0x%02x '%c' [n1:%td - n0:%td = length:%td] total:%td", [state]{
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
							cn1 = ((n1 + 1) >= nf || (n1 + 1) < ni) ? '\0' : *(n1 + 1);
							cn2 = ((n1 + 2) >= nf || (n1 + 2) < ni) ? '\0' : *(n1 + 2);
							if (cn1 == cmd_prefix && (cn2 == '_' || (cn2 >= 'A' && cn2 <= 'Z') || (cn2 >= 'a' && cn2 <= 'z'))) {
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
						cp1 = ((n1 - 1) >= nf || (n1 - 1) < ni) ? '\0' : *(n1 - 1);
						cn1 = ((n1 + 1) >= nf || (n1 + 1) < ni) ? '\0' : *(n1 + 1);
						if (cn == ':' && (cn1 == ':' || cp1 == ':' || cp1 == '/')) {
							break;
						} else {
							cn = '\0';
						}
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
	char cn, cn1, cp1;
	size_t length;
	const char *ni = path.data();
	const char *nf = ni + path.size();
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
		L_URL_PARSER(CYAN + "3 ->> %3s 0x%02x '%c' [n1:%td - n0:%td = length:%td] total:%td", [state]{
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
						cp1 = ((n1 - 1) >= nf || (n1 - 1) < ni) ? '\0' : *(n1 - 1);
						cn1 = ((n1 + 1) >= nf || (n1 + 1) < ni) ? '\0' : *(n1 + 1);
						if (cn1 == ':' || cp1 == ':' || cp1 == '/') {
							break;
						} else {
							assert(n1 >= n0);
							length = n1 - n0;
							if (!length) {
								return State::INVALID_NSP;
							}
							off_nsp = n0;
							len_nsp = length;
							state = State::PTH;
							n0 = n1 + 1;
						}
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


void
PathParser::skip_id() noexcept
{
	off_id = 0;
}


std::string_view
PathParser::get_pth()
{
	if (!off_pth) return "";
	return std::string_view(off_pth, len_pth);
}


std::string_view
PathParser::get_hst()
{
	if (!off_hst) return "";
	return std::string_view(off_hst, len_hst);
}


std::string_view
PathParser::get_nsp()
{
	if (!off_nsp) return "";
	return std::string_view(off_nsp, len_nsp);
}


std::string_view
PathParser::get_pmt()
{
	if (!off_pmt) return "";
	return std::string_view(off_pmt, len_pmt + (off_ppmt ? 0 : len_ppmt));
}


std::string_view
PathParser::get_ppmt()
{
	if (!off_ppmt) return "";
	return std::string_view(off_ppmt, len_ppmt - 1);
}


std::string_view
PathParser::get_cmd()
{
	if (!off_cmd) return "";
	return std::string_view(off_cmd, len_cmd);
}


std::string_view
PathParser::get_id()
{
	if (!off_id) return "";
	return std::string_view(off_id, len_id);
}


#ifdef L_URL_PARSER_DEFINED
#undef L_URL_PARSER_DEFINED
#undef L_URL_PARSER
#endif
