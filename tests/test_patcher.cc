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

#include "test_patcher.h"

#include "../src/msgpack_patcher.h"
#include "../src/rapidjson/rapidjson.h"
#include "../src/rapidjson/document.h"
#include "utils.h"


int test_mix() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_mix.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string expected;
	filename = "examples/json/patch_result.txt";
	if (!read_file_contents(filename, &expected)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	rapidjson::Document doc_patch;
	rapidjson::Document doc_obj;
	json_load(doc_patch, patch_str);
	json_load(doc_obj, obj_str);

	MsgPack patch(doc_patch);
	MsgPack obj(doc_obj);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_add() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_add.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\", \"name\":\"Bruce Wayne\", \"super_power\":\"High-tech equipment and weapons\", \"enemy\":\"Joker\", \"creation\":\"1939\", \"partnerships\":\"Robin\"}, {\"hero\":\"Superman\", \"name\":\"Clark Kent\", \"super_power\":\"too many\", \"enemy\":\"Lex Luthor\", \"creation\":\"1933\"}, {\"hero\":\"Flash\", \"name\":\"Bart Allen\", \"super_power\":\"fast\", \"enemy\":\"Zoom\", \"creation\":\"1940\"}, {\"hero\":\"Green Lantern\", \"name\":\"Hal Jordan\", \"super_power\":\"Use of power ring\", \"enemy\":\"The Gambler\", \"creation\":\"1940\"}], \"villains\":[{\"villain\":\"Joker\", \"name\":\"unknown\", \"super_power\":\"Genius-level intellect\", \"enemy\":\"Batman\", \"creation\":\"1940\"}, {\"villain\":\"Mr. Freeze\", \"name\":\"Dr. Victor Fries\", \"super_power\":\"Sub-zero physiology\", \"enemy\":\"Batman\", \"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_remove() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_remove.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\", \"name\":\"Bruce Wayne\", \"super_power\":\"High-tech equipment and weapons\", \"enemy\":\"Joker\"}, {\"hero\":\"Superman\", \"name\":\"Clark Kent\", \"super_power\":\"too many\", \"enemy\":\"Lex Luthor\", \"creation\":\"1933\"}, {\"hero\":\"Flash\", \"name\":\"Bart Allen\", \"super_power\":\"fast\", \"enemy\":\"Zoom\", \"creation\":\"1940\"}], \"villains\":[{\"villain\":\"Joker\", \"name\":\"unknown\", \"super_power\":\"Genius-level intellect\", \"enemy\":\"Batman\", \"creation\":\"1940\"}, {\"villain\":\"Mr. Freeze\", \"name\":\"Dr. Victor Fries\", \"super_power\":\"Sub-zero physiology\", \"enemy\":\"Batman\", \"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_replace() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_replace.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\", \"name\":\"Bruce Wayne\", \"super_power\":\"High-tech equipment and weapons\", \"enemy\":\"Riddler\", \"creation\":\"1939\"}, {\"hero\":\"Superman\", \"name\":\"Clark Kent\", \"super_power\":\"too many\", \"enemy\":\"Lex Luthor\", \"creation\":\"1933\"}, {\"hero\":\"Flash\", \"name\":\"Bart Allen\", \"super_power\":\"fast\", \"enemy\":\"Zoom\", \"creation\":\"1940\"}], \"villains\":[{\"villain\":\"Joker\", \"name\":\"unknown\", \"super_power\":\"Genius-level intellect\", \"enemy\":\"Batman\", \"creation\":\"1940\"}, {\"villain\":\"Mr. Freeze\", \"name\":\"Dr. Victor Fries\", \"super_power\":\"Sub-zero physiology\", \"enemy\":\"Batman\", \"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_move() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_move.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\", \"name\":\"Bruce Wayne\", \"super_power\":\"High-tech equipment and weapons\", \"creation\":\"1939\"}, {\"hero\":\"Superman\", \"name\":\"Clark Kent\", \"super_power\":\"too many\", \"enemy\":\"Joker\", \"creation\":\"1933\"}, {\"hero\":\"Flash\", \"name\":\"Bart Allen\", \"super_power\":\"fast\", \"enemy\":\"Zoom\", \"creation\":\"1940\"}], \"villains\":[{\"villain\":\"Joker\", \"name\":\"unknown\", \"super_power\":\"Genius-level intellect\", \"enemy\":\"Batman\", \"creation\":\"1940\"}, {\"villain\":\"Mr. Freeze\", \"name\":\"Dr. Victor Fries\", \"super_power\":\"Sub-zero physiology\", \"enemy\":\"Batman\", \"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_copy() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_copy.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\", \"name\":\"Bruce Wayne\", \"super_power\":\"High-tech equipment and weapons\", \"enemy\":\"Joker\", \"creation\":\"1939\"}, {\"hero\":\"Superman\", \"name\":\"Clark Kent\", \"super_power\":\"too many\", \"enemy\":\"Lex Luthor\", \"creation\":\"1933\", \"other_enemy\":\"Joker\"}, {\"hero\":\"Flash\", \"name\":\"Bart Allen\", \"super_power\":\"fast\", \"enemy\":\"Zoom\", \"creation\":\"1940\"}], \"villains\":[{\"villain\":\"Joker\", \"name\":\"unknown\", \"super_power\":\"Genius-level intellect\", \"enemy\":\"Batman\", \"creation\":\"1940\"}, {\"villain\":\"Mr. Freeze\", \"name\":\"Dr. Victor Fries\", \"super_power\":\"Sub-zero physiology\", \"enemy\":\"Batman\", \"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_test() {
	std::string obj_str;
	std::string filename("examples/json/object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	std::string patch_str;
	filename = "examples/json/patch_test.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR(nullptr, "Can not read the file %s", filename.c_str());
		RETURN(1);
	}

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		RETURN(0);
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_incr() {
	std::string obj_str("{ \"age\" : 24 }");
	std::string patch_str("[ { \"op\":\"incr\", \"path\":\"/age\", \"value\": \"1\", \"limit\": \"26\"} ]");
	std::string expected("{\"age\":25}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);

	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		L(nullptr, "RESULT FOR TEST_INCR %s", result.c_str());
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}


int test_decr() {
	std::string obj_str("{ \"age\" : 24 }");
	std::string patch_str("[ { \"op\":\"decr\", \"path\":\"/age\", \"value\": 1, \"limit\": 22} ]");
	std::string expected("{\"age\":23}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);

	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR(nullptr, "ERROR: Patch is not working.\nResult:\n%s\nExpected:\n%s", result.c_str(), expected.c_str());
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const Exception& exc) {
		L_EXC(nullptr, "ERROR: %s", exc.get_context());
		RETURN(1);
	}
}
