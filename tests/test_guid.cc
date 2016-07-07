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

#include "test_guid.h"

#include "../src/guid/guid.h"
#include "utils.h"


int test_guid() {
	GuidGenerator generator;

	auto g1 = generator.newGuid();
	auto g2 = generator.newGuid();
	auto g3 = generator.newGuid();

	L_DEBUG(nullptr, "Guids generated: %s  %s  %s", g1.to_string().c_str(), g2.to_string().c_str(), g3.to_string().c_str());
	if (g1 == g2 || g1 == g3 || g2 == g3) {
		L_ERR(nullptr, "ERROR: Not all random guids are different");
		RETURN(1);
	}

	std::string u1("3c0f2be3-ff4f-40ab-b157-c51a81eff176");
	std::string u2("e47fcfdf-8db6-4469-a97f-57146dc41ced");
	std::string u3("b2ce58e8-d049-4705-b0cb-fe7435843781");

	Guid s1(u1);
	Guid s2(u2);
	Guid s3(u3);
	Guid s4(u1);

	if (s1 == s2) {
		L_ERR(nullptr, "ERROR: s1 and s2 must be different");
		RETURN(1);
	}

	if (s1 != s4) {
		L_ERR(nullptr, "ERROR: s1 and s4 must be equal");
		RETURN(1);
	}

	if (s1.to_string() != u1) {
		L_ERR(nullptr, "ERROR: string generated from s1 is wrong");
		RETURN(1);
	}

	if (s2.to_string() != u2) {
		L_ERR(nullptr, "ERROR: string generated from s2 is wrong");
		RETURN(1);
	}

	if (s3.to_string() != u3) {
		L_ERR(nullptr, "ERROR: string generated from s3 is wrong");
		RETURN(1);
	}

	RETURN(0);
}
