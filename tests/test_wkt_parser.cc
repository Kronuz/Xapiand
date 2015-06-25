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
							if (strcasecmp(trixel_exp.c_str(), (*itn).c_str()) != 0) {
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


// Test of speed
int test_wkt_speed()
{
	int repeat = 10;
	clock_t start = clock();
	std::string EWKT("POLYGON ((39 -125, 39 -120, 42 -120, 39 -120))");
	for (int i = 0; i < repeat; i++) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	LOG(NULL, "Time required for execution a single POLYGON: %f seconds\n", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10),(20 30, 35 35, 30 20, 20 30))");
	for (int i = 0; i < repeat; i++) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	LOG(NULL, "Time required for execution a POLYGON compound: %f seconds\n", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("MULTIPOINT (10 40, 40 30, 20 20, 30 10)");
	for (int i = 0; i < repeat; i++) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	LOG(NULL, "Time required for execution a MULTIPOINT: %f seconds\n", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("CIRCLE (39 -125, 10000)");
	for (int i = 0; i < repeat; i++) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	LOG(NULL, "Time required for execution a CIRCLE: %f seconds\n", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	return 0;
}