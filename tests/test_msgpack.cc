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

#define RETURN(x) { Log::finish(); return x; }


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
	RETURN(1);
#else
	RETURN(0);
#endif
}


int test_pack() {
	std::string buffer;
	if (!read_file_contents("examples/msgpack/json_test1.txt", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json_test1.txt]");
		RETURN(1);
	}

	rapidjson::Document doc;
	try {
		json_load(doc, buffer);
	} catch (const std::exception&) {
		RETURN(1);
	}

	auto obj = MsgPack(doc);

	std::string pack_expected;
	if (!read_file_contents("examples/msgpack/test1.mpack", &pack_expected)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test1.mpack]");
		RETURN(1);
	}

	if (pack_expected != obj.to_string()) {
		L_ERR(nullptr, "ERROR: MsgPack::to_MsgPack is no working correctly");
		RETURN(1);
	}

	RETURN(0);
}


int test_unpack() {
	std::string buffer;
	if (!read_file_contents("examples/msgpack/test1.mpack", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test1.mpack]");
		RETURN(1);
	}

	MsgPack obj(buffer);

	std::string expected;
	if (!read_file_contents("examples/msgpack/json_test1_unpack.txt", &expected)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json_test1_unpack.txt]");
		RETURN(1);
	}

	std::string result = obj.to_json_string();
	if (expected != result) {
		L_ERR(nullptr, "ERROR: MsgPack::unpack is not working\n\nExpected: %s\n\nResult: %s\n", expected.c_str(), result.c_str());
		RETURN(1);
	}

	RETURN(0);
}


int test_explore_json() {
	std::string buffer;
	if (!read_file_contents("examples/msgpack/test2.mpack", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test2.mpack]");
		RETURN(1);
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
		RETURN(1);
	}

	RETURN(0);
}


int test_add_items() {
	std::string expected;
	if (!read_file_contents("examples/msgpack/json_test2.txt", &expected)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json_test2.txt]");
		RETURN(1);
	}

	std::string buffer;
	if (!read_file_contents("examples/msgpack/test2.mpack", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/test2.mpack]");
		RETURN(1);
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
		RETURN(1);
	}

	RETURN(0);
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
		RETURN(1);
	}

	if (l_str.compare("\"México\"") != 0) {
		L_ERR(nullptr, "ERROR: lvalue assigment in MsgPack is not working\n\nExpected: \"México\\n\nResult: %s\n", l_str.c_str());
		RETURN(1);
	}
	RETURN(0);
}


int test_path() {
	std::string buffer;
	if (!read_file_contents("examples/json/object_path.txt", &buffer)) {
		L_ERR(nullptr, "ERROR: Can not read the file [examples/json/object_path.txt]");
		RETURN(1);
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
		RETURN(1);
	}

	if (parent.compare(parent_expected) != 0) {
		L_ERR(nullptr, "ERROR: solve path in MsgPack is not working\n\nExpected: %s\nResult: %s\n", parent_expected.c_str(), parent.c_str());
		RETURN(1);
	}

	RETURN(0);
}


int test_clone() {
	MsgPack obj;
	obj["elem1"] = "Elem1";
	obj["elem2"] = "Elem2";
	obj["elem3"] = "Elem3";

	auto copy_obj = obj.clone();

	obj["elem1"] = "Mod_Elem1";
	obj["elem2"] = "Mod_Elem2";
	obj["elem3"] = "Mod_Elem3";
	obj["elem4"] = "Elem4";
	obj["elem1"] = "Final_Elem1";
	obj["elem2"] = "Final_Elem2";
	obj["elem3"] = "Final_Elem3";
	obj["elem4"] = "Final_Elem4";

	copy_obj["elem1"] = "Copy_Elem1";
	copy_obj["elem2"] = "Copy_Elem2";
	copy_obj["elem3"] = "Copy_Elem3";
	copy_obj["elem4"] = "Copy_Elem4";
	copy_obj["elem1"] = "Final_Copy_Elem1";
	copy_obj["elem2"] = "Final_Copy_Elem2";
	copy_obj["elem3"] = "Final_Copy_Elem3";
	copy_obj["elem4"] = "Final_Copy_Elem4";

	std::string str_orig_expect("{\"elem1\":\"Final_Elem1\", \"elem2\":\"Final_Elem2\", \"elem3\":\"Final_Elem3\", \"elem4\":\"Final_Elem4\"}");
	auto str_orig = obj.to_json_string();
	if (str_orig_expect != str_orig) {
		L_ERR(nullptr, "MsgPack::clone is not working. Result: %s, Expected: %s", str_orig.c_str(), str_orig_expect.c_str());
		RETURN(1);
	}

	std::string str_copy_expect("{\"elem1\":\"Final_Copy_Elem1\", \"elem2\":\"Final_Copy_Elem2\", \"elem3\":\"Final_Copy_Elem3\", \"elem4\":\"Final_Copy_Elem4\"}");
	auto str_copy = copy_obj.to_json_string();
	if (str_copy != str_copy_expect) {
		L_ERR(nullptr, "MsgPack::clone is not working. Result: %s, Expected: %s", str_copy.c_str(), str_copy_expect.c_str());
		RETURN(1);
	}

	RETURN(0);
}


int test_erase() {
	MsgPack obj;
	obj["elem1"] = "Elem1";
	obj["elem2"] = "Elem2";
	obj["elem3"] = "Elem3";
	obj["elem4"] = "Elem4";

	obj.erase("elem1");
	obj.erase("elem3");

	try {
		obj.at("elem1");
		L_ERR(nullptr, "MsgPack::erase() is not working");
		RETURN(1);
	} catch (const std::out_of_range&) { }

	try {
		obj.at("elem3");
		L_ERR(nullptr, "MsgPack::erase() is not working");
		RETURN(1);
	} catch (const std::out_of_range&) { }

	obj["elem2"] = "Final_Elem2";
	obj["elem4"] = "Final_Elem4";

	std::string str_obj_expect("{\"elem2\":\"Final_Elem2\", \"elem4\":\"Final_Elem4\"}");
	auto str_obj = obj.to_json_string();
	if (str_obj_expect != str_obj) {
		L_ERR(stderr, "MsgPack::erase() is not working correctly. Result: %s, Expected: %s", str_obj.c_str(), str_obj_expect.c_str());
		RETURN(1);
	}

	RETURN(0);
}


int test_reserve() {
	std::string data;
	read_file_contents("examples/msgpack/test1.mpack", &data);
	MsgPack obj(data);

	size_t r_size = 128 * obj.size();
	obj.reserve(r_size);
	if (obj.capacity() != r_size) {
		L_ERR(nullptr, "MsgPack::reserve(msgpack::map) is not working. Result: %zu  Expected: %zu\n", obj.capacity(), r_size);
		RETURN(1);
	}

	if (obj.to_string() != data) {
		L_ERR(nullptr, "MsgPack::expand_map is not allocating memory correctly.\n");
		RETURN(1);
	}

	auto doc = to_json("[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10]");
	obj = doc;
	std::string orig_data = obj.to_json_string().c_str();
	r_size = 128 * obj.size();

	obj.reserve(r_size);
	if (obj.capacity() != r_size) {
		L_ERR(nullptr, "MsgPack::reserve(msgpack::array) is not working. Result: %zu  Expected: %zu\n", obj.capacity(), r_size);
		RETURN(1);
	}

	if (obj.to_json_string() != orig_data) {
		L_ERR(nullptr, "MsgPack::expand_array is not allocating memory correctly.\n");
		RETURN(1);
	}

	RETURN(0);
}


int test_reset() {
	std::string data;
	read_file_contents("examples/msgpack/test1.mpack", &data);
	MsgPack obj(data);

	MsgPack obj2;
	obj2.reset(obj);

	for (int i = 0; i < 300; ++i) {
		obj[std::to_string(i)] = i;
	}

	for (int i = 0; i < 300; ++i) {
		obj2.erase(std::to_string(i));
	}

	if (obj.capacity() != obj2.capacity()) {
		L_ERR(nullptr, "Error in MsgPack::reset, objects have differents capabilities\n");
		RETURN(1);
	}

	if (obj.size() != obj2.size()) {
		L_ERR(nullptr, "Error in MsgPack::reset, objects have differents sizes\n");
		RETURN(1);
	}

	if (obj.to_json_string() != obj2.to_json_string()) {
		L_ERR(nullptr, "Error in MsgPack::reset, objects are different\n");
		RETURN(1);
	}

	if (obj.to_string() != data) {
		L_ERR(nullptr, "Error in MsgPack::reset with inserts and deletes is not working\n");
		RETURN(1);
	}

	RETURN(0);
}


int test_explicit_constructors() {
	std::string expect_json;
	read_file_contents("examples/msgpack/json_test1_unpack.txt", &expect_json);
	int res = 0;

	// Buffer object
	std::string data;
	read_file_contents("examples/msgpack/test1.mpack", &data);
	MsgPack buf_obj(data);
	if (buf_obj.to_json_string() != expect_json) {
		L_ERR(nullptr, "MsgPack::MsgPack(std::string) is not working correctly. Result: %s\nExpected: %s\n", buf_obj.to_json_string().c_str(), expect_json.c_str());
		++res;
	}


	// rapidjson::Document
	std::string str_json;
	read_file_contents("examples/msgpack/json_test1.txt", &str_json);
	auto json_doc = to_json(str_json);
	MsgPack json_obj(json_doc);
	if (json_obj.to_json_string() != expect_json) {
		L_ERR(nullptr, "MsgPack::MsgPack(rapidjson::Document) is not working correctly. Result: %s\nExpected: %s\n", json_obj.to_json_string().c_str(), expect_json.c_str());
		++res;
	}

	// msgpack::object
	msgpack::object o(data);
	MsgPack msg_obj(o);
	if (msg_obj.get_str() != data) {
		L_ERR(nullptr, "MsgPack::MsgPack(msgpack::object) is not working correctly. Result: %s\nExpected: %s\n", msg_obj.get_str().c_str(), data.c_str());
		++res;
	}

	// msgpack::unpacked
	msgpack::unpacked u;
	msgpack::unpack(&u, data.data(), data.size());
	MsgPack unp_obj(u);
	if (unp_obj.to_json_string() != expect_json) {
		L_ERR(nullptr, "MsgPack::MsgPack(msgpack::unpacked) is not working correctly. Result: %s\nExpected: %s\n", unp_obj.to_json_string().c_str(), expect_json.c_str());
		++res;
	}

	RETURN(res);
}
