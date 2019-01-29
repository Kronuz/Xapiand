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

#include "test_msgpack.h"

#include "../src/msgpack.h"
#include "../src/split.h"
#include "utils.h"


const std::string path_test_msgpack = std::string(FIXTURES_PATH) + "/examples/";


int test_msgpack_constructors() {
	INIT_LOG
	std::string res1("[1,2,3,4,5]");
	std::string res2("[[\"one\",1],[\"two\",2],[\"three\",3],[\"four\",4],100.78,[\"five\",5,200.789],1000,true,\"str_value\"]");
	std::string res3("{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5}");
	std::string res4("{\"one\":1,\"two\":2,\"three\":{\"value\":30,\"person\":{\"name\":\"José\",\"last\":\"Perez\"}},\"four\":4,\"five\":5}");

	// List initialize ARRAY
	MsgPack o = { 1, 2, 3, 4, 5 };

	int res = 0;
	auto result = o.to_string();
	if (result != res1) {
		L_ERR("ERROR: MsgPack(array list) is not working. Result:\n %s\nExpected:\n %s\n", result, res1);
		++res;
	}

	MsgPack o2 = {
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
		100.78,
		{ "five", 5, 200.789 },
		1000,
		true,
		"str_value"
	};

	result = o2.to_string();
	if (result != res2) {
		L_ERR("ERROR: MsgPack(initialize list nested ARRAY) is not working. Result:\n %s\nExpected:\n %s\n", result, res2);
		++res;
	}

	// List initialize MAP
	MsgPack o3 = {
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
		{ "five", 5 }
	};

	result = o3.to_string();
	if (result != res3) {
		L_ERR("ERROR: MsgPack(initialize list MAP) is not working. Result:\n %s\nExpected:\n %s\n", result, res3);
		++res;
	}

	MsgPack o4 = {
		{ "one", 1 },
		{ "two", 2 },
		{ "three",
			{
				{ "value", 30 },
				{ "person",
					{
						{ "name", "José" },
						{ "last", "Perez" },
					}
				}
			}
		},
		{ "four", 4 },
		{ "five", 5 }
	};

	result = o4.to_string();
	if (result != res4) {
		L_ERR("ERROR: MsgPack(initialize list nested MAP) is not working. Result:\n %s\nExpected:\n %s\n", result, res4);
		++res;
	}

	// const MsgPack&
	MsgPack o5(o3);

	result = o5.to_string();
	if (result != res3) {
		L_ERR("ERROR: MsgPack(const MsgPack&) is not working. Result:\n %s\nExpected:\n %s\n", result, res3);
		++res;
	}

	// MsgPack&&
	auto o6(MsgPack({
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
		100.78,
		{ "five", 5, 200.789 },
		1000,
		true,
		"str_value"
	}));

	result = o6.to_string();
	if (result != res2) {
		L_ERR("ERROR: MsgPack(const MsgPack&) is not working. Result:\n %s\nExpected:\n %s\n", result, res3);
		++res;
	}

	// rapidjson::Document
	std::string str_json;
	std::string filename(path_test_msgpack);
	filename.append("msgpack/json_test1.txt");
	if (!read_file_contents(filename, &str_json)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		++res;
	} else {
		std::string expect_json;
		filename.assign(path_test_msgpack).append("msgpack/json_test1_unpack.txt");
		if (!read_file_contents(filename, &expect_json)) {
			L_ERR("ERROR: Can not read the file: %s", filename);
			++res;
		} else {
			MsgPack json_obj(to_json(str_json));
			if (json_obj.to_string() != expect_json) {
				L_ERR("MsgPack::MsgPack(rapidjson::Document) is not working correctly. Result: %s\nExpected: %s\n", repr(json_obj.to_string()), repr(expect_json));
				++res;
			}
		}
	}

	RETURN(res);
}


int test_msgpack_assigment() {
	INIT_LOG
	std::string res1("[1,2,3,4,5]");
	std::string res2("{\"one\":1,\"two\":2,\"three\":3,\"four\":4,\"five\":5}");

	MsgPack m_array({ 1, 2, 3, 4, 5 });
	MsgPack m_map({
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
		{ "five", 5 }
	});

	int res = 0;

	m_array = m_map;
	auto result = m_array.to_string();
	if (result != res2) {
		L_ERR("ERROR: Mgspack::copy assigment from ARRAY to MAP is not working. Result:\n %s\nExpected:\n %s\n", result, res2);
		++res;
	}
	if (m_array.capacity() != m_map.size()) {
		L_ERR("ERROR: Mgspack::copy assigment from ARRAY to MAP is not reserving correctly. Result:\n %zu\nExpected:\n %zu\n", m_array.capacity(), m_map.size());
		++res;
	}

	m_array = MsgPack({ 1, 2, 3, 4, 5 });
	result = m_array.to_string();
	if (result != res1) {
		L_ERR("ERROR: Mgspack::move assigment from MAP to ARRAY is not working. Result:\n %s\nExpected:\n %s\n", result, res1);
		++res;
	}
	if (m_array.capacity() != m_array.size()) {
		L_ERR("ERROR: Mgspack::move assigment from MAP to ARRAY is not reserving correctly. Result:\n %zu\nExpected:\n %zu\n", m_array.capacity(), m_array.size());
		++res;
	}

	m_map = m_array;
	result = m_map.to_string();
	if (result != res1) {
		L_ERR("ERROR: Msgpack::copy assigment from MAP to ARRAY is not working. Result:\n %s\nExpected:\n %s\n", result, res1);
		++res;
	}
	if (m_map.capacity() != m_array.size()) {
		L_ERR("ERROR: Msgpack::copy assigment from MAP to ARRAY is not reserving correctly. Result:\n %zu\nExpected:\n %zu\n", m_map.capacity(), m_array.size());
		++res;
	}

	MsgPack o5({ 1, 2, 3, 4, 5});
	result = o5.to_string();

	m_map = MsgPack({
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
		{ "five", 5 }
	});

	result = m_map.to_string();
	if (result != res2) {
		L_ERR("ERROR: Msgpack::move assigment from ARRAY to MAP is not working. Result:\n %s\nExpected:\n %s\n", result, res2);
		++res;
	}
	if (m_map.capacity() != m_map.size()) {
		L_ERR("ERROR: Msgpack::move assigment from ARRAY to MAP is not reserving correctly. Result:\n %zu\nExpected:\n %zu\n", m_map.capacity(), m_map.size());
		++res;
	}

	RETURN(res);
}


int test_msgpack_iterator() {
	INIT_LOG
	std::string expected("\"one\", 1, \"two\", 2, \"three\", 3, \"four\", 4, \"five\", 5, ");

	MsgPack o = { "one", 1, "two", 2, "three", 3, "four", 4, "five", 5 };

	std::stringstream ss;
	const auto it_e = o.cend();
	for (auto it = o.cbegin(); it != it_e; ++it) {
		ss << *it << ", ";
	}

	int res = 0;
	if (ss.str() != expected) {
		L_ERR("ERROR: MsgPack::iterator with array is not working\n\nExpected: %s\n\nResult: %s\n", expected, ss.str());
		++res;
	}

	o = {
		{ "one", 1 },
		{ "two", 2 },
		{ "three", 3 },
		{ "four", 4 },
		{ "five", 5 }
	};

	ss.str(std::string());
	const auto it_e2 = o.cend();
	for (auto it = o.cbegin(); it != it_e2; ++it) {
		ss << *it << ", " << o.at(*it) << ", ";
	}

	if (ss.str() != expected) {
		L_ERR("ERROR: MsgPack::iterator with map is not working\n\nExpected: %s\n\nResult: %s\n", expected, ss.str());
		++res;
	}

	RETURN(res);
}


int test_msgpack_serialise() {
	INIT_LOG
	std::string buffer;
	std::string filename(path_test_msgpack + "msgpack/json_test1.txt");
	if (!read_file_contents(filename, &buffer)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	rapidjson::Document doc;
	try {
		json_load(doc, buffer);
	} catch (const std::exception& exc) {
		L_EXC("ERROR: %s", exc.what());
		RETURN(1);
	}

	auto obj = MsgPack(doc);

	std::string pack_expected;
	std::string expected_filename(path_test_msgpack + "msgpack/test1.mpack");
	if (!read_file_contents(expected_filename, &pack_expected)) {
		L_ERR("ERROR: Can not read the file: %s", expected_filename);
		RETURN(1);
	}

	if (pack_expected != obj.serialise()) {
		L_ERR("ERROR: MsgPack::serialise is no working correctly");
		RETURN(1);
	}

	RETURN(0);
}


int test_msgpack_unserialise() {
	INIT_LOG
	std::string buffer;
	std::string filename(path_test_msgpack + "msgpack/test1.mpack");
	if (!read_file_contents(filename, &buffer)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	auto obj = MsgPack::unserialise(buffer);

	std::string expected;
	std::string expected_filename(path_test_msgpack + "msgpack/json_test1_unpack.txt");
	if (!read_file_contents(expected_filename, &expected)) {
		L_ERR("ERROR: Can not read the file: %s", expected_filename);
		RETURN(1);
	}

	auto result = obj.to_string();
	if (expected != result) {
		L_ERR("ERROR: MsgPack::unserialise is not working\n\nExpected: %s\n\nResult: %s\n", expected, result);
		RETURN(1);
	}

	RETURN(0);
}


int test_msgpack_explore() {
	INIT_LOG
	std::string buffer;
	std::string filename(path_test_msgpack + "msgpack/test2.mpack");
	if (!read_file_contents(filename, &buffer)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	auto obj = MsgPack::unserialise(buffer);

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
		"\"uuid\":\"e82fe710-dca6-41f3-be6c-52be4661a462\"\n"
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

	// Explore MAP.
	int res = 0;
	std::stringstream ss;
	for (const auto& x : obj) {
		ss << x << ":" << obj.at(x) << "\n";
	}

	if (ss.str() != expected) {
		L_ERR("ERROR: MsgPack [using at] does not explore the map correctly\n\nExpected: %s\n\nResult: %s\n", expected, ss.str());
		++res;
	}

	ss.str(std::string());
	for (const auto& x : obj) {
		ss << x << ":" << obj[x] << "\n";
	}

	if (ss.str() != expected) {
		L_ERR("ERROR: MsgPack [using operator[]] does not explore the map correctly\n\nExpected: %s\n\nResult: %s\n", expected, ss.str());
		++res;
	}

	// Explore ARRAY.
	const auto& range = obj["range"];
	expected = "0 1 2 3 4 5 6 7 8 9 ";
	ss.str(std::string());
	for (const auto& x : range) {
		ss << x << " ";
	}

	if (ss.str() != expected) {
		L_ERR("ERROR: MsgPack does not explore the array correctly\n\nExpected: %s\n\nResult: %s\n", expected, ss.str());
		++res;
	}

	RETURN(res);
}


int test_msgpack_copy() {
	INIT_LOG
	MsgPack obj = {
		{"elem1", "Elem1"},
		{"elem2", "Elem2"}
	};

	auto copy_obj = obj;

	obj["elem1"] = "Mod_Elem1";
	obj["elem2"] = "Mod_Elem2";
	obj["elem3"] = "Final_Elem3";
	obj["elem4"] = "Final_Elem4";
	obj["elem1"] = "Final_Elem1";
	obj["elem2"] = "Final_Elem2";

	std::string str_orig_expect("{\"elem1\":\"Final_Elem1\",\"elem2\":\"Final_Elem2\",\"elem3\":\"Final_Elem3\",\"elem4\":\"Final_Elem4\"}");

	copy_obj["elem3"] = "Final_Copy_Elem3";
	copy_obj["elem4"] = "Final_Copy_Elem4";
	copy_obj["elem1"] = "Final_Copy_Elem1";
	copy_obj["elem2"] = "Final_Copy_Elem2";

	std::string str_copy_expect("{\"elem1\":\"Final_Copy_Elem1\",\"elem2\":\"Final_Copy_Elem2\",\"elem3\":\"Final_Copy_Elem3\",\"elem4\":\"Final_Copy_Elem4\"}");

	auto str_orig = obj.to_string();
	int res = 0;
	if (str_orig_expect != str_orig) {
		L_ERR("Copy MsgPack (Original) is not working. Result: %s, Expected: %s", str_orig, str_orig_expect);
		++res;
	}

	auto str_copy = copy_obj.to_string();
	if (str_copy != str_copy_expect) {
		L_ERR("Copy MsgPack (Copy) is not working. Result: %s, Expected: %s", str_copy, str_copy_expect);
		++res;
	}

	RETURN(res);
}


int test_msgpack_reference() {
	INIT_LOG
	MsgPack obj = {
		{"elem1", "Elem1"},
		{"elem2", "Elem2"}
	};

	auto& copy_obj = obj;

	obj["elem1"] = "Mod_Elem1";
	obj["elem2"] = "Mod_Elem2";
	obj["elem3"] = "Final_Elem3";
	obj["elem4"] = "Final_Elem4";
	obj["elem1"] = "Final_Elem1";
	obj["elem2"] = "Final_Elem2";

	copy_obj["elem3"] = "Final_Copy_Elem3";
	copy_obj["elem4"] = "Final_Copy_Elem4";
	copy_obj["elem1"] = "Final_Copy_Elem1";
	copy_obj["elem2"] = "Final_Copy_Elem2";

	std::string str_expect("{\"elem1\":\"Final_Copy_Elem1\",\"elem2\":\"Final_Copy_Elem2\",\"elem3\":\"Final_Copy_Elem3\",\"elem4\":\"Final_Copy_Elem4\"}");

	auto str_orig = obj.to_string();
	int res = 0;
	if (str_expect != str_orig) {
		L_ERR("Copy MsgPack (Original) is not working. Result: %s, Expected: %s", str_orig, str_expect);
		++res;
	}

	auto str_copy = copy_obj.to_string();
	if (str_copy != str_expect) {
		L_ERR("Copy MsgPack (Copy) is not working. Result: %s, Expected: %s", str_copy, str_expect);
		++res;
	}

	RETURN(res);
}


int test_msgpack_path() {
	INIT_LOG
	std::string buffer;
	std::string filename(path_test_msgpack + "json/object_path.txt");
	if (!read_file_contents(filename, &buffer)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	rapidjson::Document doc_path;
	json_load(doc_path, buffer);
	MsgPack obj(doc_path);

	std::string path_str("/AMERICA/COUNTRY/1");
	std::vector<std::string_view> paths;
	Split<>::split(path_str, '/', std::back_inserter(paths));

	const auto& path_msgpack = obj.path(paths);

	auto target = path_msgpack.to_string();

	int res = 0;
	if (target.compare("\"MEXICO\"") != 0) {
		L_ERR("ERROR: MsgPack::path is not working\n\nExpected: \"MEXICO\"\nResult: %s\n", target);
		++res;
	}

	RETURN(res);
}


int test_msgpack_erase() {
	INIT_LOG
	// Erase by key
	MsgPack obj = {
		{ "elem1", "Elem1" },
		{ "elem2", "Elem2" },
		{ "elem3", "Elem3" },
		{ "elem4", "Elem4" }
	};

	obj.erase("elem1");
	obj.erase("elem3");

	int res = 0;
	try {
		obj.at("elem1");
		L_ERR("MsgPack::erase(key) is not working\n");
		++res;
	} catch (const std::out_of_range&) { }

	try {
		obj.at("elem3");
		L_ERR("MsgPack::erase(key) is not working\n");
		++res;
	} catch (const std::out_of_range&) { }

	obj["elem2"] = "Final_Elem2";
	obj["elem4"] = "Final_Elem4";

	std::string str_obj_expect("{\"elem2\":\"Final_Elem2\",\"elem4\":\"Final_Elem4\"}");
	auto str_obj = obj.to_string();
	if (str_obj_expect != str_obj) {
		L_ERR("ERROR: MsgPack::erase(key) is not working correctly. Result: %s, Expected: %s\n", str_obj, str_obj_expect);
		++res;
	}

	// Erase by offset
	obj = MsgPack({
		{ "elem1", "Elem1" },
		{ "elem2", "Elem2" },
		{ "elem3", "Elem3" },
		{ "elem4", "Elem4" }
	});

	obj.erase(0);
	obj.erase(2);

	try {
		obj.at("elem1");
		L_ERR("MsgPack::erase(offset) is not working\n");
		++res;
	} catch (const std::out_of_range&) { }

	try {
		obj.at("elem4");
		L_ERR("MsgPack::erase(offset) is not working\n");
		++res;
	} catch (const std::out_of_range&) { }

	obj["elem2"] = "Final_Elem2";
	obj["elem3"] = "Final_Elem3";

	str_obj_expect = "{\"elem2\":\"Final_Elem2\",\"elem3\":\"Final_Elem3\"}";
	str_obj = obj.to_string();
	if (str_obj_expect != str_obj) {
		L_ERR("ERROR: MsgPack::erase(offset) is not working correctly. Result: %s, Expected: %s\n", str_obj, str_obj_expect);
		++res;
	}

	obj = { 1, 2, 3, 4, 5 };
	obj.erase(1);
	obj.erase(2);

	str_obj_expect = "[1,3,5]";
	str_obj = obj.to_string();
	if (str_obj_expect != str_obj) {
		L_ERR("ERROR: MsgPack::erase(offset) is not working correctly. Result: %s, Expected: %s\n", str_obj, str_obj_expect);
		++res;
	}

	obj[0] = 11;
	obj[1] = 31;
	obj[2] = 51;

	str_obj_expect = "[11,31,51]";
	str_obj = obj.to_string();
	if (str_obj_expect != str_obj) {
		L_ERR("ERROR: MsgPack::erase(offset) is not working correctly. Result: %s, Expected: %s\n", str_obj, str_obj_expect);
		++res;
	}

	RETURN(res);
}


int test_msgpack_reserve() {
	INIT_LOG
	std::string data;
	std::string filename(path_test_msgpack + "msgpack/test1.mpack");
	if (!read_file_contents(filename, &data)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	auto obj = MsgPack::unserialise(data);

	int res = 0;
	size_t r_size = obj.size();
	while (r_size < 100) {
		r_size = MSGPACK_GROWTH_FACTOR * r_size;
	}
	obj.reserve(r_size);
	if (obj.capacity() != r_size) {
		L_ERR("ERROR: MsgPack::reserve(msgpack::map) is not working. Result: %zu  Expected: %zu\n", obj.capacity(), r_size);
		++res;
	}

	auto result = obj.serialise();
	if (result != data) {
		L_ERR("ERROR: MsgPack::expand_map is not allocating memory correctly. Result: %s  Expect: %s\n", result, data);
		++res;
	}

	obj = MsgPack({ 0.2, true, 2, 3, 4, 5, 6, 7, 8, 9, 10 });
	auto orig_data = obj.to_string();

	r_size = 1024;
	obj.reserve(r_size);
	if (obj.capacity() < r_size) {
		L_ERR("ERROR: MsgPack::reserve(msgpack::array) is not working. Result: %zu  Expected: %zu\n", obj.capacity(), r_size);
		++res;
	}

	if (obj.to_string() != orig_data) {
		L_ERR("MsgPack::expand_array is not allocating memory correctly.\n");
		++res;
	}

	RETURN(res);
}


int test_msgpack_keys() {
	INIT_LOG
	int res = 0;
	// Test for duplicate keys.
	try {
		MsgPack obj = {
			{ "item1", "Item1" },
			{ "item2", "Item2" },
			{ "item2", "Item3" }
		};
		L_ERR("ERROR: MsgPack must not accept duplicate keys");
		++res;
	} catch (const MsgPack::duplicate_key&) { }

	std::string data;
	std::string filename(path_test_msgpack + "msgpack/test1.mpack");
	if (!read_file_contents(filename, &data)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		++res;
	}

	MsgPack obj = MsgPack::unserialise(data);

	auto _size = obj.size();
	try {
		for (auto& key : obj) {
			key = std::string("_data");
		}
		L_ERR("ERROR: MsgPack must not accept duplicate keys");
		++res;
	} catch (const MsgPack::duplicate_key&) {
		if (_size != obj.size()) {
			++res;
		}
	}

	RETURN(res);
}


int test_msgpack_change_keys() {
	INIT_LOG
	MsgPack obj = {
		{ "item1", "Item1" },
		{ "item2", "Item2" },
		{ "item3", "Item3" },
		{ "item4", "Item4" }
	};

	int i = 1;
	for (auto& key : obj) {
		auto new_str_key = std::string("key_").append(std::to_string(i++));
		key = new_str_key;
	}

	obj["key_1"] = "Val1";
	obj["key_2"] = "Val2";
	obj["key_3"] = "Val3";
	obj["key_4"] = "Val4";

	std::string expected("{\"key_1\":\"Val1\",\"key_2\":\"Val2\",\"key_3\":\"Val3\",\"key_4\":\"Val4\"}");

	auto result = obj.to_string();
	if (result == expected) {
		RETURN(0);
	} else {
		L_ERR("Change keys in MsgPack  is not working. Result: %s\nExpected: %s\n", result, expected);
		RETURN(1);
	}
}


int test_msgpack_map() {
	INIT_LOG
	size_t r0 = 4;
	std::string f1 = "firstname";
	std::string r1 = "Marcial Alejandro";
	std::string f2 = "lastname";
	std::string r2 = "Ruiz";
	std::string f3 = "age";
	std::string r3 = "23";
	std::string f4 = "gender";
	std::string r4 = "male";

	std::string buffer;
	std::string filename(path_test_msgpack + "json/object_map_test.txt");
	if (!read_file_contents(filename, &buffer)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	rapidjson::Document doc_path;
	json_load(doc_path, buffer);
	MsgPack object(doc_path);

	object.erase("Jose");

	auto& sub_obj = object["Marcial"];

	if (sub_obj.size() != r0) {
		L_ERR("Updated the map after erase is not working. Result: %zu\nExpected: %zu\n", sub_obj.size(), r0);
	}

	try {
		sub_obj[f1];
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the map after erase is not working. Expected: %s\n", r1);
		RETURN(1);
	}
	try {
		sub_obj[f2];
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the map after erase is not working. Expected: %s\n", r2);
		RETURN(1);
	}
	try {
		sub_obj[f3];
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the map after erase is not working. Expected: %s\n", r3);
		RETURN(1);
	}
	try {
		sub_obj[f4];
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the map after erase is not working. Expected: %s\n", r4);
		RETURN(1);
	}

	RETURN(0);
}


int test_msgpack_array() {
	INIT_LOG
	size_t r0 = 5;
	std::string f1 = "villain";
	std::string e1 = "Mr. Freeze";
	std::string f2 = "name";
	std::string e2 = "Dr. Victor Fries";
	std::string f3 = "super_power";
	std::string e3 = "Sub-zero physiology";
	std::string f4 = "enemy";
	std::string e4 = "Batman";
	std::string f5 = "creation";
	std::string e5 = "1956";

	std::string buffer;
	std::string filename(path_test_msgpack + "json/object_to_patch.txt");
	if (!read_file_contents(filename, &buffer)) {
		L_ERR("ERROR: Can not read the file: %s", filename);
		RETURN(1);
	}

	rapidjson::Document doc_path;
	json_load(doc_path, buffer);
	MsgPack object(doc_path);

	object["villains"].erase(0);

	auto& sub_obj = object["villains"][0];

	if (sub_obj.size() != r0) {
		L_ERR("Updated the array after erase is not working. Result: %zu\nExpected: %zu\n", sub_obj.size(), r0);
	}

	try {
		auto& r1 = sub_obj[f1];
		try {
			if (r1.str() != e1) {
				L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: %s\n", r1.str(), e1);
			}
		} catch (const msgpack::type_error&) {
			L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: \"%s\"\n", r1.to_string(), e1);
			RETURN(1);
		}
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the array after erase is not working.\n\t  Result: <out_of_range>\n\tExpected: %s\n", e1);
		RETURN(1);
	}

	try {
		auto& r2 = sub_obj[f2];
		try {
			if (r2.str() != e2) {
				L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: %s\n", r2.str(), e2);
			}
		} catch (const msgpack::type_error&) {
			L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: \"%s\"\n", r2.to_string(), e2);
			RETURN(1);
		}
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the array after erase is not working.\n\t  Result: <out_of_range>\n\tExpected: %s\n", e2);
		RETURN(1);
	}

	try {
		auto& r3 = sub_obj[f3];
		try {
			if (r3.str() != e3) {
				L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: %s\n", r3.str(), e3);
			}
		} catch (const msgpack::type_error&) {
			L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: \"%s\"\n", r3.to_string(), e3);
			RETURN(1);
		}
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the array after erase is not working.\n\t  Result: <out_of_range>\n\tExpected: %s\n", e3);
		RETURN(1);
	}

	try {
		auto& r4 = sub_obj[f4];
		try {
			if (r4.str() != e4) {
				L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: %s\n", r4.str(), e4);
			}
		} catch (const msgpack::type_error&) {
			L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: \"%s\"\n", r4.to_string(), e4);
			RETURN(1);
		}
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the array after erase is not working.\n\t  Result: <out_of_range>\n\tExpected: %s\n", e4);
		RETURN(1);
	}

	try {
		auto& r5 = sub_obj[f5];
		try {
			if (r5.str() != e5) {
				L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: %s\n", r5.str(), e5);
			}
		} catch (const msgpack::type_error&) {
			L_ERR("Updated the array after erase is not working.\n\t  Result: %s\n\tExpected: \"%s\"\n", r5.to_string(), e5);
			RETURN(1);
		}
	} catch (const std::out_of_range& e) {
		L_ERR("Updated the array after erase is not working.\n\t  Result: <out_of_range>\n\tExpected: %s\n", e5);
		RETURN(1);
	}


	RETURN(0);
}
