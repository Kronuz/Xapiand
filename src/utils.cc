//
//  utils.cpp
//  Xapiand
//
//  Created by Germán M. Bravo on 2/24/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#include <stdio.h>

#include "pthread.h"

#include "utils.h"


bool qmtx_inited = false;
pthread_mutex_t qmtx;


std::string repr(const std::string &string)
{
	return repr(string.c_str(), string.size());
}

std::string repr(const char * p, size_t size)
{
	const char *p_end = p + size;
	std::string ret;
	char buff[4];

	while (p != p_end) {
		char c = *p++;
		if (c == 9) {
			ret += "\\t";
		} else if (c == 10) {
			ret += "\\n";
		} else if (c == 13) {
			ret += "\\r";
//		} else if (c == '"') {
//			ret += "\\\"";
		} else if (c == '\'') {
			ret += "\\\'";
		} else if (c >= ' ' && c <= '~') {
			sprintf(buff, "%c", c);
			ret += buff;
		} else {
			sprintf(buff, "\\x%02x", c & 0xff);
			ret += buff;
		}
	}
	return ret;
}


void log(void *obj, const char *format, ...)
{
	if (!qmtx_inited) {
		pthread_mutex_init(&qmtx, 0);
		qmtx_inited = true;
	}

	pthread_mutex_lock(&qmtx);

	FILE * file = stderr;
	fprintf(file, "tid(0x%lx): 0x%lx - ", (unsigned long)pthread_self(), (unsigned long)obj);
	va_list argptr;
	va_start(argptr, format);
	vfprintf(file, format, argptr);
	va_end(argptr);

	pthread_mutex_unlock(&qmtx);
}
