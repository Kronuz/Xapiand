/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "../src/wkt_parser.h"
#include "utils.h"

#include <algorithm>
#include <time.h>


const std::string path_test_wkt = std::string(PACKAGE_PATH_TEST) + "/examples/wkt/";
const std::string python_wkt = "python_files/wkt/";;


// Testing WKT parser.
// Python files are generated to view the results.
int test_wkt_parser() {
	int cont = 0;
	std::string name(path_test_wkt + "parser_tests.txt");
	std::ifstream readFile(name);
	std::string EWKT, file_expect, file_result;
	double error = 0.2;
	bool partials = true;

	// Make the path for the python files generated.
	build_path_index(python_wkt);

	if (readFile.is_open()) {
		while (std::getline(readFile, EWKT)) {
			std::getline(readFile, file_expect);
			std::getline(readFile, file_result);

			file_expect = path_test_wkt + file_expect;
			file_result = python_wkt + file_result;

			std::ifstream readEFile(file_expect);
			if (readEFile.is_open()) {
				try {
					EWKT_Parser ewkt = EWKT_Parser(EWKT, partials, error);
					for (auto itn = ewkt.trixels.begin(); itn != ewkt.trixels.end(); ++itn) {
						std::string trixel_exp;
						if (!readEFile.eof()) {
							std::getline(readEFile, trixel_exp);
							if (strcasecmp(trixel_exp.c_str(), (*itn).c_str()) != 0) {
								++cont;
								L_ERR(nullptr, "ERROR: File (%s) Result(%s) Expect(%s).", file_expect.c_str(), (*itn).c_str(), trixel_exp.c_str());
							}
						} else {
							++cont;
							L_ERR(nullptr, "ERROR: Expected less trixels.");
							readEFile.close();
							break;
						}
					}

					if (!readEFile.eof()) {
						++cont;
						L_ERR(nullptr, "ERROR: Expected more trixels.");
						readEFile.close();
						break;
					}

					// Python for the Geometry.
					HTM::writePython3D(file_result, ewkt.gv, ewkt.trixels);
				} catch (const std::exception& exc) {
					L_EXC(nullptr, "ERROR: (%s) %s", EWKT.c_str(), exc.what());
					++cont;
				}
				readEFile.close();
			} else {
				L_ERR(nullptr, "ERROR: File %s not found.", file_expect.c_str());
				++cont;
			}
		}
		readFile.close();
	} else {
		L_ERR(nullptr, "ERROR: File %s not found.", name.c_str());
		++cont;
	}

	if (cont == 0) {
		L_DEBUG(nullptr, "Testing WKT parser is correct!, run with python examples/{#}_WKT.py to see the trixels that cover the geometry.");
		RETURN(0);
	} else {
		L_ERR(nullptr, "ERROR: Testing WKT parser has mistakes.");
		RETURN(1);
	}
}


// Test of speed
int test_wkt_speed() {
	int repeat = 10;
	clock_t start;
	start = clock();
	std::string EWKT("POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10))");
	for (int i = 0; i < repeat; ++i) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	L_DEBUG(nullptr, "Time required for execution a single POLYGON: %f seconds", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("POLYGON ((35 10, 45 45, 15 40, 10 20, 35 10),(20 30, 35 35, 30 20, 20 30))");
	for (int i = 0; i < repeat; ++i) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	L_DEBUG(nullptr, "Time required for execution a compound POLYGON: %f seconds", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("CHULL ((35 10, 45 45, 15 40, 10 20, 35 10))");
	for (int i = 0; i < repeat; ++i) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	L_DEBUG(nullptr, "Time required for execution a single CHULL: %f seconds", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("CHULL ((35 10, 45 45, 15 40, 10 20, 35 10),(20 30, 35 35, 30 20, 20 30))");
	for (int i = 0; i < repeat; ++i) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	L_DEBUG(nullptr, "Time required for execution a compound CHULL: %f seconds", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("POINT (10 40)");
	for (int i = 0; i < repeat; ++i) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	L_DEBUG(nullptr, "Time required for execution a POINT: %f seconds", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	start = clock();
	EWKT = std::string("CIRCLE (39 -125, 10000)");
	for (int i = 0; i < repeat; ++i) {
		EWKT_Parser ewkt = EWKT_Parser(EWKT, true, 0.1);
	}
	L_DEBUG(nullptr, "Time required for execution a CIRCLE: %f seconds", (double)(clock() - start) / (repeat * CLOCKS_PER_SEC));

	RETURN(0);
}
