/*
 * Copyright deipi.com LLC and contributors. All rights reserved.
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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <algorithm>
#include "pthread.h"
#include "utils.h"
#include <xapian.h>

#define DATE_RE "([1-9][0-9]{3})([-/ ]?)(0[1-9]|1[0-2])\\2(0[0-9]|[12][0-9]|3[01])([T ]?([01]?[0-9]|2[0-3]):([0-5][0-9])(:([0-5][0-9])([.,]([0-9]{1,3}))?)?([ ]*[+-]([01]?[0-9]|2[0-3]):([0-5][0-9])|Z)?)?([ ]*\\|\\|[ ]*([+-/\\dyMwdhms]+))?"
#define DATE_MATH_RE "([+-]\\d+|\\/{1,2})([dyMwdhms])"
#define COORDS_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d+|\\d+)"
#define COORDS_DISTANCE_RE "(\\d*\\.\\d+|\\d+)\\s?,\\s?(\\d*\\.\\d*|\\d+)\\s?;\\s?(\\d*\\.\\d*|\\d+)(ft|in|yd|mi|km|[m]{1,2}|cm)?"
#define NUMERIC_RE "(\\d*\\.\\d+|\\d+)"
#define FIND_RANGE_RE "([^ ]*\\.\\.)"
#define FIND_ORDER_RE "([_a-zA-Z][_a-zA-Z0-9]+,[_a-zA-Z][_a-zA-Z0-9]*)"


pthread_mutex_t qmtx = PTHREAD_MUTEX_INITIALIZER;
pcre *compiled_date_re = NULL;
pcre *compiled_date_math_re = NULL;
pcre *compiled_coords_re = NULL;
pcre *compiled_coords_dist_re = NULL;
pcre *compiled_numeric_re = NULL;
pcre *compiled_find_range_re = NULL;


std::string repr(const std::string &string)
{
	return repr(string.c_str(), string.size());
}


std::string repr(const char * p, size_t size)
{
	char *buff = new char[size * 4 + 1];
	char *d = buff;
	const char *p_end = p + size;
	while (p != p_end) {
		char c = *p++;
		if (c == 9) {
			*d++ = '\\';
			*d++ = 't';
		} else if (c == 10) {
			*d++ = '\\';
			*d++ = 'n';
		} else if (c == 13) {
			*d++ = '\\';
			*d++ = 'r';
		} else if (c == '\'') {
			*d++ = '\\';
			*d++ = '\'';
		} else if (c >= ' ' && c <= '~') {
			*d++ = c;
		} else {
			*d++ = '\\';
			*d++ = 'x';
			sprintf(d, "%02x", (unsigned char)c);
			d += 2;
		}
		//printf("%02x: %ld < %ld\n", (unsigned char)c, (unsigned long)(d - buff), (unsigned long)(size * 4 + 1));
	}
	*d = '\0';
	std::string ret(buff);
	delete [] buff;
	return ret;
}


void log(void *obj, const char *format, ...)
{
	pthread_mutex_lock(&qmtx);

	FILE * file = stderr;
	pthread_t thread = pthread_self();
	char name[100];
	pthread_getname_np(thread, name, sizeof(name));
	fprintf(file, "tid(0x%lx:%s): 0x%.12lx - ", (unsigned long)thread, name, (unsigned long)obj);
	va_list argptr;
	va_start(argptr, format);
	vfprintf(file, format, argptr);
	va_end(argptr);

	pthread_mutex_unlock(&qmtx);
}

const char HEX2DEC[256] =
{
	/*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
	/* 0 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 1 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 2 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,-1,-1, -1,-1,-1,-1,

	/* 4 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 5 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 6 */ -1,10,11,12, 13,14,15,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 7 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

	/* 8 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* 9 */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* A */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* B */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,

	/* C */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* D */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* E */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1,
	/* F */ -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1, -1,-1,-1,-1
};


std::string urldecode(const char *src, size_t size)
{
	// Note from RFC1630:  "Sequences which start with a percent sign
	// but are not followed by two hexadecimal characters (0-9, A-F) are reserved
	// for future extension"

	const char * SRC_END = src + size;
	const char * SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'

	char * const pStart = new char[size];
	char * pEnd = pStart;

	while (src < SRC_LAST_DEC)
	{
		if (*src == '%')
		{
			char dec1, dec2;
			if (-1 != (dec1 = HEX2DEC[*(src + 1)])
				&& -1 != (dec2 = HEX2DEC[*(src + 2)]))
			{
				*pEnd++ = (dec1 << 4) + dec2;
				src += 3;
				continue;
			}
		}

		*pEnd++ = *src++;
	}

	// the last 2- chars
	while (src < SRC_END)
	*pEnd++ = *src++;

	std::string sResult(pStart, pEnd);
	delete [] pStart;
	//std::replace( sResult.begin(), sResult.end(), '+', ' ');
	return sResult;
}


int url_qs(const char *name, const char *qs, size_t size, parser_query_t *par)
{
	const char *nf = qs + size;
	const char *n1, *n0;
	const char *v0 = NULL;

	if(par->offset == NULL) {
		n0 = n1 = qs;
	} else {
		n0 = n1 = par->offset + par -> length;
	}

	while (1) {
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
			if(strlen(name) == n1 - n0 && strncmp(n0, name, n1 - n0) == 0) {
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
							par->offset = v0 + 1;
							par->length = v1 - v0 - 1;
							return 0;
						}
						v1++;
					}
				} else {
					par->offset = n1 + 1;
					par->length = 0;
					return 0;
				}
			} else if (!cn) {
				return -1;
			} else if (cn != '=') {
				n0 = n1 + 1;
				v0 = NULL;
			}
		}
		n1++;
	}
	return -1;
}


int url_path(const char* n1, size_t size, parser_url_path_t *par)
{
	const char *nf = n1 + size + 1;
	const char *n0, *n2 ,*r, *p = NULL;
	size_t cmd_size = 0;
	int state = 0;
	n0 = n1;

	bool other_slash = false;
	par->off_host = NULL;
	par->len_host = 0;
	par->off_command = NULL;
	par->len_command = 0;


	if(par->offset == NULL) {
		n0 = n2 = n1;
	} else {
		n0 = n2 = n1 = par->offset + par -> length + 1;
	}

	par->length = 0;
	par->offset = 0;

	while (1) {
		char cn = *n1;
		if (n1 == nf) {
			cn = '\0';
		}
		switch(cn) {
			case '\0':
				if (n0 == n1) return -1;
				if (p) {
					r = p + 1;
					while(1) {
						char cr = *r;
						if (r == nf) {
							cr = '\0';
						}
						if (!cr) break;
						switch (cr) {
							case '/':
								r++;
								continue;

							default:
								cmd_size++;
								r++;
								break;
						}
					}
					par->off_command = p + 1;
					par->len_command = cmd_size;
					par->offset = n2;
					par->length = r - n2;
				}
			case ',':
				if (!p) p = n1;
				switch (state) {
					case 0:
					case 1:
						par->off_path = n0;
						par->len_path = p - n0;
						if (cn) n1++;
						if(!par->length) {
							par->offset = n2;
							par->length = p - n2;
						}

						return 0;
					case 2:
						par->off_host = n0;
						par->len_host = p - n0;
						if (cn) n1++;
						if(!par->length) {
							par->offset = n2;
							par->length = p - n2;
						}
						return 0;
				}
				p = NULL;
				other_slash = false;
				break;

			case ':':
				switch (state) {
					case 0:
						par->off_namespace = n0;
						par->len_namespace = n1 - n0;
						state = 1;
						n0 = n1 + 1;
						break;
					default:
						state = -1;
				}
				p = NULL;
				other_slash = false;
				break;

			case '@':
				switch (state) {
					case 0:
						par->off_path = n0;
						par->len_path = n1 - n0;
						state = 2;
						n0 = n1 + 1;
						break;
					case 1:
						par->off_path = n0;
						par->len_path = n1 - n0;
						state = 2;
						n0 = n1 + 1;
						break;
					default:
						state = -1;
				}
				p = NULL;
				other_slash = false;
				break;


			case '/':
				if (*(n1 + 1) && !p && !other_slash) {
					p = n1;
					other_slash = true;
				} else if(*(n1 + 1) && *(n1 + 1) != '/') {
					p = n1;
					other_slash = true;
				}

		}
		n1++;
	}
	return -1;
}


int pcre_search(const char *subject, int length, int startoffset, int options, const char *pattern, pcre **code, group_t **groups)
{
	int erroffset;
	const char *error;

	// First, the regex string must be compiled.
	if (*code == NULL) {
		//pcre_free is not use because we use a struct pcre static and gets free at the end of the program
		LOG(NULL, "pcre compiled is NULL.\n");
		*code = pcre_compile(pattern, 0, &error, &erroffset, 0);
		if (*code == NULL) {
			LOG_ERR(NULL, "pcre_compile of %s failed (offset: %d), %s\n", pattern, erroffset, error);
			return -1;
		}
	}

	if (*code != NULL) {
		int n;
		if (pcre_fullinfo(*code, NULL, PCRE_INFO_CAPTURECOUNT, &n) != 0) {
			return -1;
		}

		if (*groups == NULL) {
			*groups = (group_t *)malloc((n + 1) * 3 * sizeof(int));
		}

		int *ocvector = (int *)*groups;
		if (pcre_exec(*code, 0, subject, length, startoffset, options, ocvector, (n + 1) * 3) >= 0) {
			return 0;
		} else return -1;
	}

	return -1;
}


std::string serialise_numeric(const std::string &field_value)
{
	double val;
	if (isNumeric(field_value)) {
		val = strtodouble(field_value);
		return Xapian::sortable_serialise(val);
	}
	return "";
}


std::string serialise_date(const std::string &field_value)
{
	std::string str_timestamp = timestamp_date(field_value);
	if (str_timestamp.size() == 0) {
		LOG_ERR(NULL, "ERROR: Format date (%s) must be ISO 8601: (eg 1997-07-16T19:20:30.451+05:00) or a epoch (double)\n", field_value.c_str());
		return "";
	}

	double timestamp = strtodouble(str_timestamp);
	LOG(NULL, "timestamp %s %f\n", str_timestamp.c_str(), timestamp);
	return Xapian::sortable_serialise(timestamp);
}


std::string unserialise_date(const std::string &serialise_val)
{
	char date[25];
	double epoch = Xapian::sortable_unserialise(serialise_val);
	time_t timestamp = (time_t) epoch;
	std::string milliseconds = std::to_string(epoch);
	milliseconds.assign(milliseconds.c_str() + milliseconds.find("."), 4);
	struct tm *timeinfo = gmtime(&timestamp);
	sprintf(date, "%.4d-%.2d-%.2dT%.2d:%.2d:%.2d%s", timeinfo->tm_year + 1900, timeinfo->tm_mon + 1, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec, milliseconds.c_str());
	return date;
}


std::string serialise_geo(const std::string &field_value)
{
	Xapian::LatLongCoords coords;
	double latitude, longitude;
	int len = (int) field_value.size(), Ncoord = 0, offset = 0;
	bool end = false;
	group_t *g = NULL;
	while (pcre_search(field_value.c_str(), len, offset, 0, COORDS_RE, &compiled_coords_re, &g) != -1) {
		std::string parse(field_value, g[1].start, g[1].end - g[1].start);
		latitude = strtodouble(parse);
		parse.assign(field_value, g[2].start, g[2].end - g[2].start);
		longitude = strtodouble(parse);
		Ncoord++;
		try {
			coords.append(Xapian::LatLongCoord(latitude, longitude));
		} catch (Xapian::Error &e) {
			LOG_ERR(NULL, "Latitude or longitude out-of-range\n");
			return "";
		}
		LOG(NULL, "Coord %d: %f, %f\n", Ncoord, latitude, longitude);
		if (g[2].end == len) {
			end = true;
			break;
		}
		offset = g[2].end;
	}

	if (g) {
		free(g);
		g = NULL;
	}

	if (Ncoord == 0 || !end) {
		LOG_ERR(NULL, "ERROR: %s must be an array of doubles [lat, lon, lat, lon, ...]\n", field_value.c_str());
		return "";
	}
	return coords.serialise();
}


std::string unserialise_geo(const std::string &serialise_val)
{
	Xapian::LatLongCoords coords;
	coords.unserialise(serialise_val);
	Xapian::LatLongCoordsIterator it = coords.begin();
	std::stringstream ss;
	for (; it != coords.end(); it++) {
		ss << (*it).latitude;
		ss << "," << (*it).longitude << ",";
	}
	std::string _coords = ss.str();
	return std::string(_coords, 0, _coords.size() - 1);
}


std::string serialise_bool(const std::string &field_value)
{
	if (!field_value.c_str()) {
		return "f";
	} else if(field_value.size() > 1) {
		if (strcasecmp(field_value.c_str(), "TRUE") == 0) {
			return "t";
		} else if (strcasecmp(field_value.c_str(), "FALSE") == 0) {
			return "f";
		} else {
			return "t";
		}
	} else {
		switch (tolower(field_value.at(0))) {
			case '1':
				return "t";
			case '0':
				return "f";
			case 't':
				return "t";
			case 'f':
				return "f";
			default:
				return "t";
		}
	}
}


std::string stringtoupper(const std::string &str)
{
	std::string tmp = str;

	struct TRANSFORM {
		char operator() (char c) { return  toupper(c);}
	};

	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM());

	return tmp;
}


std::string stringtolower(const std::string &str)
{
	std::string tmp = str;

	struct TRANSFORM {
		char operator() (char c) { return  tolower(c);}
	};

	std::transform(tmp.begin(), tmp.end(), tmp.begin(), TRANSFORM());

	return tmp;
}


std::string prefixed(const std::string &term, const std::string &prefix)
{
	if (isupper(term.at(0))) {
		if (prefix.size() == 0) {
			return term;
		} else {
			return prefix + ":" + term;
		}
	}

	return prefix + term;
}


unsigned int get_slot(const std::string &name)
{
	if (stringtolower(name).compare("id") == 0) return 0;

	std::string standard_name;
	if (strhasupper(name)) {
		standard_name = stringtoupper(name);
	} else {
		standard_name = name;
	}
	std::string _md5(md5(standard_name), 24, 8);
	unsigned int slot = hex2int(_md5);
	if (slot == 0x00000000) {
		slot = 0x00000001;
	} else if (slot == 0xffffffff) {
		slot = 0xfffffffe;
	}
	return slot;
}


unsigned int hex2int(const std::string &input)
{
	unsigned int n;
	std::stringstream ss;
	ss << std::hex << input;
	ss >> n;
	ss.flush();
	return n;
}


int strtoint(const std::string &str)
{
	int number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


double strtodouble(const std::string &str)
{
	double number;
	std::stringstream ss;
	ss << std::dec << str;
	ss >> number;
	ss.flush();
	return number;
}


std::string timestamp_date(const std::string &str)
{
	int len = (int) str.size();
	int ret, n[7], offset = 0;
	std::string oph, opm;
	double  timestamp;
	group_t *gr = NULL;

	ret = pcre_search(str.c_str(), len, offset, 0, DATE_RE, &compiled_date_re, &gr);

	if (ret != -1 && len == (gr[0].end - gr[0].start)) {
		std::string parse(str, gr[1].start, gr[1].end - gr[1].start);
		n[0] = strtoint(parse);
		parse.assign(str, gr[3].start, gr[3].end - gr[3].start);
		n[1] = strtoint(parse);
		parse.assign(str, gr[4].start, gr[4].end - gr[4].start);
		n[2] = strtoint(parse);
		if (gr[5].end - gr[5].start > 0) {
			parse.assign(str, gr[6].start, gr[6].end - gr[6].start);
			n[3] = strtoint(parse);
			parse.assign(str, gr[7].start, gr[7].end - gr[7].start);
			n[4] = strtoint(parse);
			if (gr[8].end - gr[8].start > 0) {
				parse.assign(str, gr[9].start, gr[9].end - gr[9].start);
				n[5] = strtoint(parse);
				if (gr[10].end - gr[10].start > 0) {
					parse.assign(str, gr[11].start, gr[11].end - gr[11].start);
					n[6] = strtoint(parse);
				} else {
					n[6] = 0;
				}
			} else {
				n[5] =  n[6] = 0;
			}
			if (gr[12].end - gr[12].start > 1) {
				if (std::string(str, gr[13].start - 1, 1).compare("+") == 0) {
					oph = "-" + std::string(str, gr[13].start, gr[13].end - gr[13].start);
					opm = "-" + std::string(str, gr[14].start, gr[14].end - gr[14].start);
				} else {
					oph = "+" + std::string(str, gr[13].start, gr[13].end - gr[13].start);
					opm = "+" + std::string(str, gr[14].start, gr[14].end - gr[14].start);
				}
				calculate_date(n, oph, "h");
				calculate_date(n, opm, "m");
			}
		} else {
			n[3] = n[4] = n[5] = n[6] = 0;
		}

		if (!validate_date(n)) {
			return "";
		}

		//Processing Date Math
		std::string date_math;
		len = gr[16].end - gr[16].start;
		if (len != 0) {
			date_math.assign(str, gr[16].start, len);
			if (gr) {
				free(gr);
				gr = NULL;
			}
			while (pcre_search(date_math.c_str(), len, offset, 0, DATE_MATH_RE, &compiled_date_math_re, &gr) == 0) {
				offset = gr[0].end;
				calculate_date(n, std::string(date_math, gr[1].start, gr[1].end - gr[1].start), std::string(date_math, gr[2].start, gr[2].end - gr[2].start));
			}
			if (gr) {
				free(gr);
				gr = NULL;
			}
			if (offset != len) {
				LOG(NULL, "ERROR: Date Math is used incorrectly.\n");
				return "";
			}
		}
		time_t tt = 0;
		struct tm *timeinfo = gmtime(&tt);
		timeinfo->tm_year   = n[0] - 1900;
		timeinfo->tm_mon    = n[1] - 1;
		timeinfo->tm_mday   = n[2];
		timeinfo->tm_hour   = n[3];
		timeinfo->tm_min    = n[4];
		timeinfo->tm_sec    = n[5];
		const time_t dateGMT = timegm(timeinfo);
		timestamp = (double) dateGMT;
		timestamp += n[6]/1000.0;
		return std::to_string(timestamp);
	}

	if (gr) {
		free(gr);
		gr = NULL;
	}

	if (isNumeric(str)) {
		return str;
	}

	return "";
}


std::string get_prefix(const std::string &name, const std::string &prefix)
{
	std::string slot = get_slot_hex(name);

	struct TRANSFORM {
		char operator() (char c) { return  c + 17;}
	};

	std::transform(slot.begin(), slot.end(), slot.begin(), TRANSFORM());

	return prefix + slot;
}


std::string get_slot_hex(const std::string &name)
{
	std::string standard_name;
	if (strhasupper(name)) {
		standard_name = stringtoupper(name);
	} else {
		standard_name = name;
	}
	std::string _md5(md5(standard_name), 24, 8);
	return stringtoupper(_md5);
}


bool strhasupper(const std::string &str)
{
	std::string::const_iterator it(str.begin());
	for ( ; it != str.end(); it++) {
		if (isupper(*it)) return true;
	}

	return false;
}


int get_coords(const std::string &str, double *coords)
{
	std::stringstream ss;
	group_t *g = NULL;
	int offset = 0, len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, offset, 0, COORDS_DISTANCE_RE, &compiled_coords_dist_re, &g);
	while (ret != -1 && (g[0].end - g[0].start) == len) {
		offset = g[0].end;
		/*LOG(NULL,"group[1] %s\n" , std::string(str.c_str() + g[1].start, g[1].end - g[1].start).c_str());
		 LOG(NULL,"group[2] %s\n" , std::string(str.c_str() + g[2].start, g[2].end - g[2].start).c_str());
		 LOG(NULL,"group[3] %s\n" , std::string(str.c_str() + g[3].start, g[3].end - g[3].start).c_str());*/
		ss.clear();
		ss << std::string(str.c_str() + g[1].start, g[1].end - g[1].start);
		ss >> coords[0];
		ss.clear();
		ss << std::string(str.c_str() + g[2].start, g[2].end - g[2].start);
		ss >> coords[1];
		ss.clear();
		ss << std::string(str.c_str() + g[3].start, g[3].end - g[3].start);
		ss >> coords[2];
		if (g[4].end - g[4].start > 0) {
			std::string units(str.c_str() + g[4].start, g[4].end - g[4].start);
			if (units.compare("mi") == 0) {
				coords[2] *= 1609.344;
			} else if (units.compare("km") == 0) {
				coords[2] *= 1000;
			} else if (units.compare("yd") == 0) {
				coords[2] *= 0.9144;
			} else if (units.compare("ft") == 0) {
				coords[2] *= 0.3048;
			} else if (units.compare("in") == 0) {
				coords[2] *= 0.0254;
			} else if (units.compare("cm") == 0) {
				coords[2] *= 0.01;
			} else if (units.compare("mm") == 0) {
				coords[2] *= 0.001;
			}
		}
		return 0;
	}
	return -1;
}


bool isRange(const std::string &str)
{
	group_t *gr = NULL;
	int ret = pcre_search(str.c_str(), (int)str.size(), 0, 0, FIND_RANGE_RE, &compiled_find_range_re , &gr);
	if (ret != -1) {
		if (gr) {
			free(gr);
			gr = NULL;
		}
		return true;
	}
	return false;
}


bool isLatLongDistance(const std::string &str)
{
	group_t *gr = NULL;
	int len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, 0, 0, COORDS_DISTANCE_RE, &compiled_coords_dist_re, &gr);
	if (ret != -1 && (gr[0].end - gr[0].start) == len) {
		if (gr) {
			free(gr);
			gr = NULL;
		}
		return true;
	}
	return false;
}


bool isNumeric(const std::string &str)
{
	group_t *g = NULL;
	int len = (int)str.size();
	int ret = pcre_search(str.c_str(), len, 0, 0, NUMERIC_RE, &compiled_numeric_re, &g);
	if (ret != -1 && (g[0].end - g[0].start) == len) {
		if (g) {
			free(g);
			g = NULL;
		}
		return true;
	}
	return false;
}


bool StartsWith(const std::string &text, const std::string &token)
{
	if (text.length() < token.length())
		return false;
	return (text.compare(0, token.length(), token) == 0);
}


void calculate_date(int n[], const std::string &op, const std::string &units)
{
	int num = strtoint(std::string(op.c_str() + 1, op.size())), max_days;
	time_t tt, dateGMT;
	struct tm *timeinfo;
	if (op.at(0) == '+' || op.at(0) == '-') {
		switch (units.at(0)) {
			case 'y':
				(op.at(0) == '+') ? n[0] += num : n[0] -= num; break;
			case 'M':
				if (op.at(0) == '+') {
					n[1] += num;
				} else {
					n[1] -= num;
				}
				tt = 0;
				timeinfo = gmtime(&tt);
				timeinfo->tm_year   = n[0] - 1900;
				timeinfo->tm_mon    = n[1] - 1;
				dateGMT = timegm(timeinfo);
				timeinfo = gmtime(&dateGMT);
				max_days = number_days(timeinfo->tm_year, n[1]);
				if (n[2] > max_days) n[2] = max_days;
				break;
			case 'w':
				(op.at(0) == '+') ? n[2] += 7 * num : n[2] -= 7 * num; break;
			case 'd':
				(op.at(0) == '+') ? n[2] += num : n[2] -= num; break;
			case 'h':
				(op.at(0) == '+') ? n[3] += num : n[3] -= num; break;
			case 'm':
				(op.at(0) == '+') ? n[4] += num : n[4] -= num; break;
			case 's':
				(op.at(0) == '+') ? n[5] += num : n[5] -= num; break;
		}
	} else {
		switch (units.at(0)) {
			case 'y':
				if (op.compare("/") == 0) {
					n[1] = 12;
					n[2] = number_days(n[0], 12);
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[1] = n[2] = 1;
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'M':
				if (op.compare("/") == 0) {
					n[2] = number_days(n[0], n[1]);
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[2] = 1;
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'w':
				tt = 0;
				timeinfo = gmtime(&tt);
				timeinfo->tm_year   = n[0] - 1900;
				timeinfo->tm_mon    = n[1] - 1;
				timeinfo->tm_mday   = n[2];
				dateGMT = timegm(timeinfo);
				timeinfo = gmtime(&dateGMT);
				if (op.compare("/") == 0) {
					n[2] += 6 - timeinfo->tm_wday;
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[2] -= timeinfo->tm_wday;
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'd':
				if (op.compare("/") == 0) {
					n[3] = 23;
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[3] = n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'h':
				if (op.compare("/") == 0) {
					n[4] = n[5] = 59;
					n[6] = 999;
				} else {
					n[4] = n[5] = n[6] = 0;
				}
				break;
			case 'm':
				if (op.compare("/") == 0) {
					n[5] = 59;
					n[6] = 999;
				} else {
					n[5] = n[6] = 0;
				}
				break;
			case 's':
				if (op.compare("/") == 0) {
					n[6] = 999;
				} else {
					n[6] = 0;
				}
			break;
		}
	}

	// Calculate new date
	tt = 0;
	timeinfo = gmtime(&tt);
	timeinfo->tm_year   = n[0] - 1900;
	timeinfo->tm_mon    = n[1] - 1;
	timeinfo->tm_mday   = n[2];
	timeinfo->tm_hour   = n[3];
	timeinfo->tm_min    = n[4];
	timeinfo->tm_sec    = n[5];
	dateGMT = timegm(timeinfo);
	timeinfo = gmtime(&dateGMT);
	n[0] = timeinfo->tm_year + 1900;
	n[1] = timeinfo->tm_mon + 1;
	n[2] = timeinfo->tm_mday;
	n[3] = timeinfo->tm_hour;
	n[4] = timeinfo->tm_min;
	n[5] = timeinfo->tm_sec;
}


bool validate_date(int n[])
{
	if (n[0] >= 1582) { //Gregorian calendar
		if (n[1] == 2 && !((n[0] % 4 == 0 && n[0] % 100 != 0) || n[0] % 400 == 0) && n[2] > 28) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 28 days\n");
			return false;
		} else if(n[1] == 2 && ((n[0] % 4 == 0 && n[0] % 100 != 0) || n[0] % 400 == 0) && n[2] > 29) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 29 days\n");
			return false;
		}
	} else {
		if (n[1] == 2 && n[0] % 4 != 0 && n[2] > 28) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 28 days\n");
			return false;
		} else if(n[1] == 2 && n[0] % 4 == 0 && n[2] > 29) {
			LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 29 days\n");
			return false;
		}
	}

	if((n[1] == 4 || n[1] == 6 || n[1] == 9 || n[1] == 11) && n[2] > 30) {
		LOG_ERR(NULL, "ERROR: Incorrect Date, This month only has 30 days\n");
		return false;
	}

	return true;
}


int number_days(int year, int month)
{
	if (year >= 1582) { //Gregorian calendar
		if (month == 2 && !((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
			return 28;
		} else if(month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
			return 29;
		}
	} else {
		if (month == 2 && year % 4 != 0) {
			return 28;
		} else if(month == 2 && year % 4 == 0) {
			return 29;
		}
	}

	if(month == 4 || month == 6 || month == 9 || month == 11) {
		return 30;
	}

	return 31;
}


std::string
unserialise(char field_type, const std::string &field_name, const std::string &serialise_val)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			return std::to_string(Xapian::sortable_unserialise(serialise_val));
		case DATE_TYPE:
			return unserialise_date(serialise_val);
		case GEO_TYPE:
			return unserialise_geo(serialise_val);
		case BOOLEAN_TYPE:
			return (serialise_val.at(0) == 'f') ? "false" : "true";
		case TEXT_TYPE:
		case STRING_TYPE:
			return serialise_val;
	}
	return "";
}


std::string
serialise(char field_type, const std::string &field_name, const std::string &field_value)
{
	switch (field_type) {
		case NUMERIC_TYPE:
			return serialise_numeric(field_value);
		case DATE_TYPE:
			return serialise_date(field_value);
		case GEO_TYPE:
			return serialise_geo(field_value);
		case BOOLEAN_TYPE:
			return serialise_bool(field_value);
		case TEXT_TYPE:
		case STRING_TYPE:
			return field_value;
	}
	return "";
}


int
identify_cmd(std::string &commad)
{
	if(!is_digits(commad)) {
		if(strcasecmp(commad.c_str(), "_search") == 0) {
			return CMD_SEARCH;
		} else if(strcasecmp(commad.c_str(), "_facets") == 0) {
			return CMD_FACETS;
		} else if(strcasecmp(commad.c_str(), "_stats") == 0) {
			return CMD_STATS;
		}
		return -1;
	} else return 0;
}

bool is_digits(const std::string &str)
{
	return std::all_of(str.begin(), str.end(), ::isdigit);
}