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

#include "test_wkt_parser.h"


// Testing WKT parser.
// Python files are generated to view the results.
int test_wkt_parser()
{
	int cont = 0;
	std::string name("examples/Tests_parser_WKT.txt");
	std::ifstream readFile(name);
	std::string EWKT, file_expect, file_result;
	char files[50];
	char output[50];
	double error = 0.2;
	bool partials = true;

	if (readFile.is_open()) {
		while (std::getline(readFile, EWKT)) {
			std::getline(readFile, file_expect);
			std::getline(readFile, file_result);

			std::ifstream readEFile(file_expect);
			if (readEFile.is_open()) {
				try {
					EWKT_Parser ewkt = EWKT_Parser(EWKT, partials, error);

					std::vector<std::string>::const_iterator itn = ewkt.trixels.begin();
					for ( ;itn != ewkt.trixels.end(); itn++) {
						std::string trixel_exp;
						if (!readEFile.eof()) {
							std::getline(readEFile, trixel_exp);
							if (trixel_exp.compare(*itn) != 0) {
								cont++;
								LOG_ERR(NULL, "ERROR: File (%s) Result(%s) Expect(%s).\n", file_expect.c_str(), (*itn).c_str(), trixel_exp.c_str());
							}
						} else {
							cont++;
							LOG_ERR(NULL, "ERROR: Expected less trixels.\n");
							break;
						}
					}

					if (!readEFile.eof()) {
						cont++;
						LOG_ERR(NULL, "ERROR: Expected more trixels.\n");
						break;
					}

					// Python for the Geometry.
					Constraint c;
					Geometry g(c);
					HTM _htm(partials, error, g);
					_htm.writePython3D(file_result, ewkt.gv, ewkt.trixels);
				} catch(const std::exception &e) {
					LOG_ERR(NULL, "ERROR: %s\n", e.what());
					cont++;
				}
			} else {
				LOG_ERR(NULL, "ERROR: File %s not found.\n", file_expect.c_str());
				cont ++;
			}
		}
	} else {
		LOG_ERR(NULL, "ERROR: File %s not found.\n", name.c_str());
		cont ++;
	}

	if (cont == 0) {
		LOG(NULL, "Testing WKT parser is correct!, run with python examples/{#}_WKT.py to see the trixels that cover the geometry.\n");
		return 0;
	} else {
		LOG_ERR(NULL, "ERROR: Testing WKT parser has mistakes.\n");
		return 1;
	}
}