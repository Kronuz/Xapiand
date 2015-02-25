//
//  utils.cpp
//  Xapiand
//
//  Created by Germán M. Bravo on 2/24/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#include <stdio.h>

#include "utils.h"

void print_string(const std::string &string) {
	fprint_string(stdout, string);
}

void fprint_string(FILE * file, const std::string &string) {
	const char *p = string.c_str();
	const char *p_end = p + string.size();
	fprintf(file, "'");
	while (p != p_end) {
		char c = *p++;
		if (c == 10) {
			fprintf(file, "\\n");
		} else if (c == 13) {
			fprintf(file, "\\r");
		} else if (c >= ' ' && c <= '~') {
			fprintf(file, "%c", c);
		} else {
			fprintf(file, "\\x%02x", c & 0xff);
		}
	}
	fprintf(file, "'\n");
}
