/*
 * Copyright (C) 2015, 2016 deipi.com LLC and contributors. All rights reserved.
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

#include "test_msgpack.h"

#include "../src/msgpack.h"
#include "../src/log.h"
#include "../src/database_utils.h"
#include "../src/utils.h"

#include <fstream>
#include <sstream>
#include <vector>


bool write_file_contents(const std::string& filename, const std::string& contents) {
	std::ofstream of(filename.data(), std::ios::out | std::ios::binary);
	if (of.bad())
		return false;
	of.write(contents.data(), contents.size());
	return true;
}


bool read_file_contents(const std::string& filename, std::string* contents) {
	std::ifstream in(filename.data(), std::ios::in | std::ios::binary);
	if (in.bad()) {
		return false;
	}

	in.seekg(0, std::ios::end);
	contents->resize(static_cast<size_t>(in.tellg()));
	in.seekg(0, std::ios::beg);
	in.read(&(*contents)[0], contents->size());
	in.close();
	return true;
}


int test_correct_cpp() {
#if defined(MSGPACK_USE_CPP03)
	L_ERR(nullptr, "ERROR: It is running c++03");
	return 1;
#else
	return 0;
#endif
}


int test_pack() {
	std::string buffer;
	if (!read_file_contents("examples/msgpack/json_test1.txt", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json_test1.txt]");
		return 1;
	}

	rapidjson::Document doc;
	try {
		json_load(doc, buffer);
	} catch (const std::exception&) {
		return 1;
	}

	auto obj = MsgPack(doc);

	std::string pack_expected;
	if (!read_file_contents("examples/msgpack/test1.mpack", &pack_expected)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test1.mpack]");
		return 1;
	}

	if (pack_expected != obj.to_string()) {
		L_ERR(nullptr, "ERROR: MsgPack::to_MsgPack is no working correctly");
		return 1;
	}

	return 0;
}


int test_unpack() {
	std::string buffer;
	if (!read_file_contents("examples/msgpack/test1.mpack", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test1.mpack]");
		return 1;
	}

	MsgPack obj(buffer);

	std::string expected;
	if (!read_file_contents("examples/msgpack/json_test1_unpack.txt", &expected)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json_test1_unpack.txt]");
		return 1;
	}

	std::string result = obj.to_json_string();
	if (expected != result) {
		L_ERR(nullptr, "ERROR: MsgPack::unpack is not working\n\nExpected: %s\n\nResult: %s\n", expected.c_str(), result.c_str());
		return 1;
	}

	return 0;
}


int test_explore_json() {
	std::string buffer;
	if (!read_file_contents("examples/msgpack/test2.mpack", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test2.mpack]");
		return 1;
	}

	MsgPack obj(buffer);

	std::string expected(
		"\"_id\":\"56892c5e23700e297bd84cd5\"\n"
		"\"about\":\"Minim ad irure pariatur nulla dolore occaecat ipsum. Qui ipsum enim aute do labore deserunt enim eu nulla duis cupidatat id est. Id cupidatat nostrud ad nulla culpa veniam nulla consequat enim sunt qui id enim. Aliquip ut deserunt irure consequat irure in fugiat. Esse veniam adipisicing deserunt culpa veniam consectetur qui ex amet. Commodo aute sit esse incididunt adipisicing non enim. Aliqua consectetur officia eiusmod veniam et amet qui adipisicing dolore voluptate reprehenderit anim commodo nulla.\"\n"
		"\"address\":\"422 Whitney Avenue, Walker, Arizona, 7324\"\n"
		"\"age\":29\n"
		"\"balance\":\"$2,952.99\"\n"
		"\"company\":\"PYRAMI\"\n"
		"\"email\":\"serena.joyner@pyrami.net\"\n"
		"\"eyeColor\":\"green\"\n"
		"\"favoriteFruit\":\"banana\"\n"
		"\"friends\":[3, {\"id\":1, \"name\":\"Norma Salas\"}]\n"
		"\"greeting\":\"Hello, Serena! You have 6 unread messages.\"\n"
		"\"guid\":\"e82fe710-dca6-41f3-be6c-52be4661a462\"\n"
		"\"index\":0\n"
		"\"isActive\":false\n"
		"\"latitude\":\"39.106713\"\n"
		"\"longitude\":\"75.253735\"\n"
		"\"name\":{\"first\":\"Jeremy\", \"last\":\"Joyner\"}\n"
		"\"phone\":\"+1 (859) 576-2384\"\n"
		"\"picture\":nil\n"
		"\"range\":[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]\n"
		"\"registered\":\"Thursday, September 4, 2014 1:27 PM\"\n"
		"\"tags\":[7, \"eiusmod\"]\n"
	);

	std::stringstream ss;
	for (auto x : obj) {
		ss << x << ":" << obj[x] << "\n";
	}

	if (ss.str() != expected) {
		L_ERR(nullptr, "ERROR: MsgPack does not explore the json correctly\n\nExpected: %s\n\nResult: %s\n", expected.c_str(), ss.str().c_str());
		return 1;
	}

	return 0;
}


int test_add_items() {
	std::string expected;
	if (!read_file_contents("examples/msgpack/json_test2.txt", &expected)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json_test2.txt]");
		return 1;
	}

	std::string buffer;
	if (!read_file_contents("examples/msgpack/test2.mpack", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test2.mpack]");
		return 1;
	}

	MsgPack obj(buffer);

	obj["name"]["middle"]["other"] = "Jeremy";
	obj["range"][30] = "Other";
	obj["company"] = "DEIPI";
	obj["branch"] = "Morelia";
	obj["country"] = "México";

	std::string result = obj.to_json_string();
	if (expected != result) {
		L_ERR(nullptr, "ERROR: Add items with MsgPack is not working\n\nExpected: %s\n\nResult: %s\n", expected.c_str(), result.c_str());
		return 1;
	}

	return 0;
}


int test_assigment() {
	MsgPack o;
	o["country"] = "México";
	MsgPack aux = o["country"];

	MsgPack r_assigment = o["country"];
	MsgPack l_assigment = aux;

	std::string r_str = r_assigment.to_json_string();
	std::string l_str = l_assigment.to_json_string();
	if (r_str.compare("\"México\"") != 0) {
		L_ERR(nullptr, "ERROR: rvalue assigment in MsgPack is not working\n\nExpected: \"México\"\nResult: %s\n", r_str.c_str());
		return 1;
	}

	if (l_str.compare("\"México\"") != 0) {
		L_ERR(nullptr, "ERROR: lvalue assigment in MsgPack is not working\n\nExpected: \"México\\n\nResult: %s\n", l_str.c_str());
		return 1;
	}
	return 0;
}


int test_path() {
	std::string buffer;
	if (!read_file_contents("examples/json/object_path.txt", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json/object_path.txt]");
		return 1;
	}

	rapidjson::Document doc_path;
	json_load(doc_path, buffer);
	MsgPack obj(doc_path);

	std::string path_str("/AMERICA/COUNTRY/1");
	std::vector <std::string> path;
	stringTokenizer(path_str, "/", path);

	MsgPack path_msgpack = obj.path(path);

	std::string target = path_msgpack.to_json_string();
	std::string parent = path_msgpack.parent().to_json_string();
	std::string parent_expected ("[\"EU\", \"MEXICO\", \"CANADA\", \"BRAZIL\"]");

	if (target.compare("\"MEXICO\"") != 0) {
		L_ERR(nullptr, "ERROR: solve path in MsgPack is not working\n\nExpected: \"MEXICO\"\nResult: %s\n", target.c_str());
		return 1;
	}

	if (parent.compare(parent_expected) != 0) {
		L_ERR(nullptr, "ERROR: solve path in MsgPack is not working\n\nExpected: %s\nResult: %s\n", parent_expected.c_str(), parent.c_str());
		return 1;
	}

	return 0;
}


int test_clone() {
	MsgPack obj;
	obj["elem1"] = "Elem1";
	obj["elem2"] = "Elem2";
	obj["elem3"] = "Elem3";

	auto copy_obj = obj.clone();

	obj["elem4"] = "Elem4";
	auto str_orig = obj.to_json_string();

	copy_obj["elem1"] = "New Elem1";
	copy_obj["elem2"] = "New Elem2";
	copy_obj["elem3"] = "New Elem3";
	copy_obj["elem4"] = "New Elem4";

	auto str_final = obj.to_json_string();
	if (str_orig != str_final) {
		L_ERR(nullptr, "MsgPack::clone is not working. Result: %s, Expected: %s", str_final.c_str(), str_orig.c_str());
		return 1;
	}

	std::string str_copy_expect("{\"elem1\":\"New Elem1\", \"elem2\":\"New Elem2\", \"elem3\":\"New Elem3\", \"elem4\":\"New Elem4\"}");
	auto str_copy = copy_obj.to_json_string();
	if (str_copy != str_copy_expect) {
		L_ERR(nullptr, "MsgPack::clone is not working. Result: %s, Expected: %s", str_copy.c_str(), str_copy_expect.c_str());
		return 1;
	}

	return 0;
}
