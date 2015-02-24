//
//  utils.cpp
//  Xapiand
//
//  Created by Germán M. Bravo on 2/24/15.
//  Copyright (c) 2015 Germán M. Bravo. All rights reserved.
//

#include "utils.h"

void print_string(const std::string &string) {
	const char *p = string.c_str();
	const char *p_end = p + string.size();
	printf("'");
	while (p != p_end) {
		if (*p >= ' ' && *p <= '~') {
			printf("%c", *p++);
		} else {
			printf("\\x%02x", *p++ & 0xff);
		}
	}
	printf("'\n");
}
