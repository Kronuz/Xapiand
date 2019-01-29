/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include "test_hash.h"

#include "../src/hash/md5.h"
#include "../src/hash/sha256.h"
#include "utils.h"


int test_md5() {
	INIT_LOG
	std::vector<std::string> test({
		"Hola Mundo",
		"Como estas",
		"The MD5 (Message-Digest Algorithm) is crypthographic hash function.",
		"MD5 is used in to check data integrity and in security applications."
	});
	std::vector<std::string> expect({
		"d501194c987486789bb01b50dc1a0adb",
		"388cab773ac8c0b110cf58252d737633",
		"8f6761bea95239db3d6464602a18c9cf",
		"c5fbb835d78b653e385ffa5e808b944e"
	});

	MD5 md5;
	for (auto it = test.begin(), rit = expect.begin(); it != test.end(); ++it, ++rit) {
		std::string res = md5(*it);
		if (res != *rit) {
			L_ERR("ERROR: Testing MD5 Failed.\nResult MD5(%s)=%s  Expected=%s", *it, res, *rit);
			RETURN (1);
		}
	}

	RETURN(0);
}


int test_sha256() {
	INIT_LOG
	std::vector<std::string> test({
		"Hola Mundo",
		"Como estas",
		"The SHA (Secure Hash Algorithm) is one of a number of cryptographic hash functions.",
		"SHA-256 is one of the successor hash functions to SHA-1, and is one of the strongest hash functions available."
	});
	std::vector<std::string> expects({
		"c3a4a2e49d91f2177113a9adfcb9ef9af9679dc4557a0a3a4602e1bd39a6f481",
		"e5e474ee8d00379c8eeae95014fcc048882f7eb7de5535c5ab3da503f0313c29",
		"881e342e0d231f921b83f42713accb00b63e1ee2d773c4abe54d872304daf718",
		"caeaa40c362aa6b2fbd64e3121be8ae83ee7efac6574ee23b1e461124104e922"
	});

	SHA256 sha256;
	for (auto it = test.begin(), rit = expects.begin(); it != test.end(); ++it, ++rit) {
		std::string res = sha256(*it);
		if (res != *rit) {
			L_ERR("ERROR: Testing SHA256 Failed.\nResult SHA256(%s)=%s  Expected=%s", *it, res, *rit);
			RETURN(1);
		}
	}

	RETURN(0);
}
