/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "test_haystack.h"

#include "../src/haystack.h"
#include "../src/log.h"

#include <fcntl.h>
#include <string.h>


int test_haystack() {
	ssize_t length;

	did_t id = 1;
	cookie_t cookie = 0x4f4f;

	Haystack writable_haystack(".", true);
	const char data[] = "Hello World";
	HaystackIndexedFile wf = writable_haystack.open(id, cookie, O_APPEND);
	length = wf.write(data, sizeof(data));
	wf.commit();
	if (length != sizeof(data)) {
		L_ERR(nullptr, "ERROR: Haystack::write is not working");
		return 1;
	}

	// The following should be asynchronous:
	writable_haystack.flush();

	Haystack haystack(".");
	char buffer[100];
	HaystackIndexedFile rf = haystack.open(id, cookie);
	length = rf.read(buffer, sizeof(buffer));

	if (length != sizeof(data)) {
		L_ERR(nullptr, "ERROR: Haystack::read is not working");
		return 1;
	}

	if (strncmp(buffer, data, sizeof(buffer)) != 0) {
		L_ERR(nullptr, "ERROR: Haystack is not working");
		return 1;
	}

	// // Walk files (id == 0, cookie == 0):
	// rf = haystack.open(0, 0);
	// while((length = rf.read(buffer, sizeof(buffer))) >= 0) {
	// 	if (length) {
	// 		fprintf(stderr, "  %zd: %s (%u:%u) at %u\n", length, buffer, rf.id(), rf.cookie(), rf.offset());
	// 	} else {
	// 		rf.next();
	// 	}
	// }
	// fprintf(stderr, "  %zd, %d\n", length, errno);

	return 0;
}
